/*************************************************** 
Pretty crappy coffee roasting code.  That is all
Written by Nathan Slattengren.  
BSD license, all text above must be included in any redistribution
This is revision 8 of the design and supports connecting to a convection oven.  See Revision 7 for the last support of coffee roaster.
****************************************************/

#include <SPI.h>
#include <Wire.h>
#include "Adafruit_MAX31855.h"
#include "LCD16x2.h"
#include "coffee_roaster.h"

LCD16x2 lcd;

int PreviousButton;
int CurrentButton;

// Default connection is using software SPI, but comment and uncomment one of
// the two examples below to switch between software SPI and hardware SPI:

// Example creating a thermocouple instance with software SPI on any three
// digital IO pins.
#define MAXDO   0
#define MAXCS1  1
#define MAXCS2  4
#define MAXCLK 30 // The TX LED has a defined Arduino pin
#define RELAY_PIN 6
#define FAN_PIN 5
#define MOTOR_PIN 11
#define LED_GREEN 8

// initialize the Thermocouple
Adafruit_MAX31855 thermocouple1(MAXCLK, MAXCS1, MAXDO);
Adafruit_MAX31855 thermocouple2(MAXCLK, MAXCS2, MAXDO);

//Defines
float Setpoint;

//For Serial recieve
const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data
boolean newData = false;

//Run a fake temperature increase
boolean simulation = false;
float Input_simulation = 75;

unsigned long startMillis;
unsigned long now;  //Register for the current time at the start of each loop

#if defined(ARDUINO_ARCH_SAMD)
// for Zero, output on USB Serial console, remove line below if using programming port to program the Zero!
 #define Serial SerialUSB
#endif

void setup() {
  Wire.begin();
  Serial.begin(115200);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(MAXCLK, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  analogWrite(RELAY_PIN, 0);
  analogWrite(FAN_PIN, fan_val);
  analogWrite(MOTOR_PIN, motor_val);

  //Flash the start screen
  display_start();

    noInterrupts();           // disable all interrupts
    /** initialize timer1 The point of the timer is to give 250ms trigger
    Caluclation is 1/16MHz * 15625 * 256 = 250ms
    **/
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;
    OCR1A = 15625;            // compare match register 16MHz/256/4Hz
    TCCR1B |= (1 << WGM12);   // CTC mode
    TCCR1B |= (1 << CS12);    // 256 prescaler 
    TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
    interrupts();  //allows interrupts

  startMillis = millis();  //initial start time for ADC sample routine
  
  
  //wait for MAX chip to stabilize if simulation is not happening
  if (simulation) {
    delay(1000);  
    lcd.lcdClear();
  }
}

boolean roasting_started = false;
boolean cooling_started = false;
unsigned long time_start = 0;
int count = 0;
int SaveState = 0;
boolean LEDBlink = true;
boolean RELAY = false;
int x1 = 0;

void loop() {
  digitalWrite(LED_GREEN, LEDBlink);
  LEDBlink = !LEDBlink;
  analogWrite(MOTOR_PIN, motor_val);
  analogWrite(FAN_PIN, fan_val);
  now = millis();//Grab time at start of loop

  //Routine to sample the ADC every 250ms.  The MAX31855 takes up to 100ms per sample to have stable data
  if(interrupt_trigger_flag = true){
//    if(sample_temp1 == true){
      Input1_average = get_temp(Input1,true);
      Serial.println(Input1_average);
      x1 = round(Input1_average);
      sprintf(Input_buffer,"%3d", x1);
//      Serial.print("Temp 1: ");
//      Serial.println(Input1);
//      }
//    else{
      Input2_average = get_temp(Input2,false);
      Serial.println(Input2_average);

//      }
//    sample_temp1 = !sample_temp1;
    
    //Go ahead and write values to LCD
    refreshlcd();
    interrupt_trigger_flag = false;
  }
  
  if (check_buttons_flag >= 1){
    check_buttons_flag = 0;
    if (button_changed_flag == 1){
      CurrentButton = 15;
      button_changed_flag = 0;
    }
    else {
      PreviousButton = CurrentButton;
      CurrentButton = lcd.readButtons();
      if (CurrentButton != PreviousButton) {button_changed_flag = 1;      
      }
    }
  }
  else CurrentButton = 15;
  
  switch (CurrentState) {
    case state_idle:{
      analogWrite(RELAY_PIN, 0);
      RELAY = false;
  
      if (CurrentButton == 14) CurrentState = state_idle_transition;
      else CurrentState = state_idle;
      
      thermal_power_button();
    }
    break;
    
    case state_idle_transition: {
      lcd.lcdClear();
      CurrentState = state_preroast;
      idle_state_flag = false;
      fan_val = 255;
      motor_val = 0;
    }
    break;

    case state_preroast: {
      if (roasting_started == false) {
        roasting_started = true;
        time_start = now;
        }
      else roast_time = (now - time_start);
      analogWrite(RELAY_PIN, roast_val);
      RELAY = true;

      thermal_power_button();
      if (!(CurrentButton & 0x02)) CurrentState = state_preroast_transition;
      else CurrentState = state_preroast;
    }
    break;

    case state_preroast_transition:
      lcd.lcdClear();
      preroast_state_flag = false;
      roasting_started = false;
      CurrentState = state_roasting;
      roast_time = 0;
      motor_val = 255;
    break;
   
    case state_roasting: {
      if (roasting_started == false) {
        roasting_started = true;
        time_start = now;
        }
      else roast_time = (now - time_start);
      analogWrite(RELAY_PIN, roast_val);
      RELAY = true;
      
      thermal_power_button();

      if (!(CurrentButton & 0x02)) CurrentState = state_roasting_transition;
      else CurrentState = state_roasting;
    }
    break;

    case state_roasting_transition:
      lcd.lcdClear();
      roasting_state_flag = false;
      roasting_started = false;
      CurrentState = state_cooling;
      roast_time = 0;
    break;
   
    case state_cooling: {
      analogWrite(RELAY_PIN, 0);
      RELAY = false;
      Setpoint = 45;

      if (cooling_started == false) {
        cooling_started = true;
        time_start = now;
      }
      else cool_time = (now - time_start);

      thermal_power_button();
      
      if(Input1 > Setpoint){
        if (!(CurrentButton & 0x02)) CurrentState = state_cooling_transition;
        else CurrentState = state_cooling;
        }
      else if((Input1 < Setpoint) || (Input1 == Setpoint)) CurrentState = state_cooling_transition;
      else CurrentState = state_cooling;
      } 
    break;

    case state_cooling_transition:
      lcd.lcdClear();
      cooling_started = false;
      cooling_state_flag = false;
      CurrentState = state_idle;
      cool_time = 0;
      fan_val = 0;
      motor_val = 0;
    break;
  }
  recvWithEndMarker();
  ProcessData();
//      Serial.print("Temp 1: ");
//      Serial.println(Input1);
}

void display_start() {
  lcd.lcdClear();
    
  lcd.lcdGoToXY(3,1);
  lcd.lcdWrite("Barrel Man v11");

  lcd.lcdGoToXY(1,2);
  lcd.lcdWrite("Coffee Roasting");
  delay(750);
}

void thermal_power_button(){
  if(!(CurrentButton & SECOND_BUTTON)) {
    roast_val++;
    sprintf(roast, "%3d", roast_val);
  }
  else if (!(CurrentButton & FIRST_BUTTON)) {
    roast_val--;
    sprintf(roast, "%3d", roast_val);
  }
}

void fan_button(){
  if(!(CurrentButton & FOURTH_BUTTON)) {
    fan_val++;
    sprintf(fan, "%3d", fan_val);
  }
  else if (!(CurrentButton & THIRD_BUTTON)) {
    fan_val--;
    sprintf(fan, "%3d", fan_val);
  }
}

//This function is to convert the millis to a readable min:sec format
char conv_currtime_disp(unsigned long input_milli){
  unsigned long allSeconds=input_milli/1000;
  int secsRemaining=allSeconds%3600;
  int runMinutes=secsRemaining/60;
  int runSeconds=secsRemaining%60;
  if (roasting_started == true) sprintf(roast_time_char,"%02d:%02d",runMinutes,runSeconds);
  else if (cooling_started == false) sprintf(cool_time_char,"%02d:%02d",runMinutes,runSeconds);
}

void recvWithEndMarker() {
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
    
    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();
        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) ndx = numChars - 1;
        }
        else {
            receivedChars[ndx] = '\0'; // terminate the string
            ndx = 0;
            newData = true;
            String thisvariable(receivedChars);
            ArtisanCommand = thisvariable;
        }
    }
}

void ProcessData() {
    if (newData == true) {
      if (ArtisanCommand.indexOf("CHAN:")==0) Serial.println("#OK");
      else if (ArtisanCommand.indexOf("UNITS:")==0) {
        if (ArtisanCommand.substring(6,7)=="F"){   //Change to Farenheit
          unit_F = true;
          Serial.println("#OK Farenheit");
          }
        else if (ArtisanCommand.substring(6,7)=="C"){  //Change to Celsius
          unit_F = false;
          Serial.println("#OK Celsius");
          }
      }
      else if (ArtisanCommand.indexOf("READ")==0) Command_READ(); //Send Temps
      else if (ArtisanCommand.indexOf("FILT")==0) Serial.println("#OK");
      }
      newData = false;
}

//Send Data
void Command_READ(){    
    Serial.print("0.00,"); //ambient
    Serial.print(Input1);
    Serial.print(",");
    Serial.print(Input2);
    Serial.println(",0.00,0.00");
}

ISR(TIMER1_COMPA_vect) {
  if (unit_F == true) {
    Input1 = thermocouple1.readFahrenheit();
    Input2 = thermocouple2.readFahrenheit();
  }
  else {
    Input1 = thermocouple1.readInternal();
    Input2 = thermocouple2.readInternal();
  }
  interrupt_trigger_flag = true;
  check_buttons_flag++;
}

//The state flags are set so the second time around it doesn't refresh the entire display
void refreshlcd(){
    if (CurrentState == state_idle) {
      display_idle();
      idle_state_flag = true;
    }
    else if (CurrentState == state_preroast) {
      conv_currtime_disp(roast_time);
      display_preroast();
      preroast_state_flag = true;
    }   
    else if (CurrentState == state_roasting) {
      conv_currtime_disp(roast_time);
      display_roasting();
      roasting_state_flag = true;
    }
    else if (CurrentState == state_cooling) {
      conv_currtime_disp(cool_time);
      display_cooling();
      cooling_state_flag = true;
    }
}

void display_idle() {
    if (idle_state_flag == false) {
    lcd.lcdGoToXY(1,1);
    lcd.lcdWrite("Idle  ");
    lcd.lcdGoToXY(7,1);
    lcd.lcdWrite("Temp = ");
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite("Start? ");
    lcd.lcdGoToXY(8,2);
    lcd.lcdWrite("PWR = ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
}

void display_preroast() {
  if (preroast_state_flag == false){
    lcd.lcdGoToXY(1,1);
    lcd.lcdWrite("Heat  ");
    lcd.lcdGoToXY(7,1);
    lcd.lcdWrite("Temp = ");
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time_char);
    lcd.lcdGoToXY(6,2);
    lcd.lcdWrite(" End?");
    lcd.lcdGoToXY(12,2);
    lcd.lcdWrite("  ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time_char);
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
}

void display_roasting() {
  if (roasting_state_flag == false){
    lcd.lcdGoToXY(1,1);
    lcd.lcdWrite("Roast ");
    lcd.lcdGoToXY(7,1);
    lcd.lcdWrite("Temp = ");
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time_char);
    lcd.lcdGoToXY(6,2);
    lcd.lcdWrite(" Stop?");
    lcd.lcdGoToXY(12,2);
    lcd.lcdWrite("  ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time_char);
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
}

void display_cooling() {
  if (cooling_state_flag == false){
    lcd.lcdGoToXY(1,1);
    lcd.lcdWrite("Cool ");
    lcd.lcdGoToXY(7,1);
    lcd.lcdWrite("Temp = ");
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(cool_time_char);
    lcd.lcdGoToXY(6,2);
    lcd.lcdWrite(" Stop?");
    lcd.lcdGoToXY(12,2);
    lcd.lcdWrite("  ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(cool_time_char);
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
}

float get_temp(float temp,boolean read_sensor_one) {
  float c;
  if (!simulation){
      if(read_sensor_one == true) {
        Input1_sample2 = Input1_sample1;
        Input1_sample1 = Input1;
        c = (Input1+Input1_sample1+Input1_sample2)/3;
      }
      else {
        Input2_sample2 = Input2_sample1;
        Input2_sample1 = Input2;
        c = (Input2+Input2_sample1+Input2_sample2)/3;  
      }
  }
  else c = simulation_temp();
  return c;
}
    
float simulation_temp(void) {
  if (CurrentState == 0) {
    count = 0;
    if ((Input_simulation > 0) || (Input_simulation < 78)) Input_simulation = 77;
    else Input_simulation = (Input_simulation - Input_simulation/9);
  }
    else {
    if (RELAY == false) {
      if (Input_simulation > 77 && Input_simulation < 100 )       Input_simulation = Input_simulation-0.0089;
      else if (Input_simulation > 100 && Input_simulation < 125 ) Input_simulation = Input_simulation-0.0128;
      else if (Input_simulation > 125 && Input_simulation < 150 ) Input_simulation = Input_simulation-0.0321;
      else if (Input_simulation > 150 && Input_simulation < 200 ) Input_simulation = Input_simulation-0.081;
      else if (Input_simulation > 200 && Input_simulation < 250 ) Input_simulation = Input_simulation-0.12;
      else if (Input_simulation > 250 && Input_simulation < 300 ) Input_simulation = Input_simulation-0.14;
      else if (Input_simulation > 300 && Input_simulation < 350 ) Input_simulation = Input_simulation-0.16;
      else if (Input_simulation > 350 && Input_simulation < 400 ) Input_simulation = Input_simulation-0.18;
      else if (Input_simulation > 400 && Input_simulation < 450 ) Input_simulation = Input_simulation-0.2;
      else if (Input_simulation > 450 && Input_simulation < 500 ) Input_simulation = Input_simulation-0.22;
      else Input_simulation = Input_simulation;
    }
    else {
      if (Input_simulation < 100)                                Input_simulation = Input_simulation + 0.024;
      else if (Input_simulation > 100 && Input_simulation < 150) Input_simulation = Input_simulation + 0.022;
      else if (Input_simulation > 150 && Input_simulation < 200) Input_simulation = Input_simulation + 0.018;
      else if (Input_simulation > 200 && Input_simulation < 250) Input_simulation = Input_simulation + 0.014;
      else if (Input_simulation > 250 && Input_simulation < 300) Input_simulation = Input_simulation + 0.0081;
      else if (Input_simulation > 300 && Input_simulation < 350) Input_simulation = Input_simulation + 0.0061;
      else if (Input_simulation > 350 && Input_simulation < 375) Input_simulation = Input_simulation + 0.00321;
      else if (Input_simulation > 375 && Input_simulation < 400) Input_simulation = Input_simulation + 0.00128;
      else if (Input_simulation > 400 && Input_simulation < 425) Input_simulation = Input_simulation + 0.00089;
      else if (Input_simulation > 425 && Input_simulation < 500) Input_simulation = Input_simulation + 0.000357;
      else Input_simulation = Input_simulation;
      }
    }
  return Input_simulation;
}
