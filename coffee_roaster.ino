/*************************************************** 
Pretty crappy coffee roasting code.  That is all
Written by Nathan Slattengren.  
BSD license, all text above must be included in any redistribution
This is revision 9 of the design and supports connecting rotisserie oven.  See Revision 7 for the last support of coffee roaster.
****************************************************/

#include <SPI.h>
#include <Wire.h>
#include "Adafruit_MAX31855.h"
#include "LCD16x2.h"
#include "coffee_roaster.h"

LCD16x2 lcd;
int buttons;

// Default connection is using software SPI, but comment and uncomment one of
// the two examples below to switch between software SPI and hardware SPI:

// Example creating a thermocouple instance with software SPI on any three
// digital IO pins.
#define MAXDO   0
#define MAXCS   1
#define MAXCLK 30 // The TX LED has a defined Arduino pin
#define RELAY_PIN 6
#define FAN_PIN 11
#define MOTOR_PIN 5
#define LED_GREEN_BOTTOM 8
#define LED_GREEN_MIDDLE 9
#define LED_RED_TOP 10


// initialize the Thermocouple
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);

//Define Variables we'll be connecting to
double Setpoint, Output, temperature;

//Run a fake temperature increase
boolean simulation = false;
boolean linear = true;
double Input_simulation = 75;

unsigned long windowStartTime;
unsigned long startMillis;
unsigned long now;  //Register for loop time

#if defined(ARDUINO_ARCH_SAMD)
// for Zero, output on USB Serial console, remove line below if using programming port to program the Zero!
 #define Serial SerialUSB
#endif

void setup() {
  windowStartTime = millis();
  Wire.begin();
  Serial.begin(19200);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(MAXCLK, OUTPUT);
  pinMode(LED_GREEN_BOTTOM, OUTPUT);
  pinMode(LED_GREEN_MIDDLE, OUTPUT);
  pinMode(LED_RED_TOP, OUTPUT);

  analogWrite(RELAY_PIN, 0);
  analogWrite(FAN_PIN, fan_val);
  analogWrite(MOTOR_PIN, motor_val);
  digitalWrite(LED_GREEN_MIDDLE, HIGH);
  digitalWrite(LED_GREEN_BOTTOM, HIGH);

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

int roasting_started = 0;
int cooling_started = 0;
unsigned long time_start = 0;
double count = 0;
int SaveState = 0;
int LEDBlink = 1;
int RELAY = 0;
int x1 = 0;

void loop() {
  digitalWrite(LED_RED_TOP, LEDBlink);
  LEDBlink = !LEDBlink;
  analogWrite(MOTOR_PIN, motor_val);
  analogWrite(FAN_PIN, fan_val);
  now = millis();//Grab time at start of loop
  
  if (check_buttons_flag >= 1){
    buttons = lcd.readButtons();
    check_buttons_flag = 0;
  }
  else buttons = 15;

  //Routine to sample the ADC every 100ms.  The reason for 100ms
  if(now - startMillis >= ADC_sample_period){
    Input = get_temp();
    x1 = round(Input);
    if (Input < 10) {
      sprintf(Input_buffer,"  %d", x1);
    }
    else if (Input <= 99) {
      sprintf(Input_buffer," %d", x1);
    }
    else if (Input > 99) {
      sprintf(Input_buffer,"%d", x1);
    }
    ADC_sample_flag = 0;
    startMillis = now;
  }
  
  //Go ahead and write values to LCD
  refreshlcd();
  
  switch (CurrentState) {
    case state_idle:{
      analogWrite(RELAY_PIN, 0);
      RELAY = 0;
      motor_val = 0;
  
      if(buttons == 15) CurrentState = state_idle;
      else if (buttons == 14) CurrentState = state_idle_transition;
      else if (buttons == 3) CurrentState = state_idle_to_debug_transition;
      else CurrentState = state_idle;
      
      thermal_power_button();
    }
    break;
    
    case state_idle_transition: {
      lcd.lcdClear();
      CurrentState = state_roasting;
      idle_state_flag = 0;
      fan_val = 255;
      fan[4] = "255";
      //fan1_ramp_up(4);
      motor_val = 255;
    }
    break;

    case state_idle_to_debug_transition:{
      lcd.lcdClear();
      CurrentState = state_debug;
      idle_state_flag = 0;
      char roast[4] = "00";
      int roast_val = 0;
      int fan_val = 0;
      //fan1_ramp_up(4);
    }
    break;
    
    case state_roasting: {
      if (roasting_started == 0) {
        roasting_started = 1;
        time_start = now;
        }
      else roast_time = (now - time_start);
      analogWrite(RELAY_PIN, roast_val);
      RELAY = 1;
      
      thermal_power_button();
      
      if (!(buttons & 0x02)) CurrentState = state_roasting_transition;
      else CurrentState = state_roasting;
    }
    break;

    case state_roasting_transition:
      lcd.lcdClear();
      roasting_state_flag = 0;
      roasting_started = 0;
      CurrentState = state_cooling;
      roast_time = 0;
    break;
   
    case state_cooling: {
      analogWrite(RELAY_PIN, 0);
      RELAY = 0;
      //analogWrite(FAN_PIN, 255);
      Setpoint = 45;

      if (cooling_started == 0) {
        cooling_started = 1;
        time_start = now;
      }
      else{
        cool_time = (now - time_start);
      }

      thermal_power_button();
      
      if(Input > Setpoint){
        if (!(buttons & 0x02))
          CurrentState = state_cooling_transition;
        else
          CurrentState = state_cooling;
      }
      else if((Input < Setpoint) || (Input == Setpoint))
        CurrentState = state_cooling_transition;
      else
        CurrentState = state_cooling;
    } 
    break;

    case state_cooling_transition:
      lcd.lcdClear();
      cooling_started = 0;
      cooling_state_flag = 0;
      CurrentState = state_idle;
      cool_time = 0;
      fan_val = 0;
      fan[4] = "  0";
      motor_val = 0;
    break;

    case state_debug: {
      analogWrite(RELAY_PIN, roast_val);
      RELAY = 1;
      //analogWrite(FAN_PIN, fan_val);

      if(buttons == 12) CurrentState = state_debug_transition;
      else CurrentState = state_debug;

      thermal_power_button();
      fan_button();
    }
    break;

    case state_debug_transition:
      lcd.lcdClear();
      debug_state_flag = 0;
      CurrentState = state_cooling;
    break;
  }
}

void display_start() {
  lcd.lcdClear();
    
  lcd.lcdGoToXY(4,1);
  lcd.lcdWrite("Barrel Man");

  lcd.lcdGoToXY(1,2);
  lcd.lcdWrite("Coffee Roasting");
  delay(750);
}

void thermal_power_button(){
  if(!(buttons & 0x08)) {
    if (roast_val == 255) roast_val = 00;
    else roast_val++;
    if (roast_val < 10) sprintf(roast, "  %d",roast_val);
    else if (roast_val < 100) sprintf(roast, " %d",roast_val);
    else sprintf(roast, "%d", roast_val);
  }
  else if (!(buttons & 0x04)) {
    if (roast_val == 00) roast_val = 255;
    else roast_val--;
    if (roast_val < 10) sprintf(roast, "  %d",roast_val);
    else if (roast_val < 100) sprintf(roast, " %d",roast_val);
    else sprintf(roast, "%d", roast_val);
  }
}

void fan_button(){
  if(!(buttons & 0x02)) {
    if (fan_val == 255) fan_val = 00;
    else fan_val++;
    if (fan_val < 10) sprintf(fan, "  %d",fan_val);
    else if (fan_val < 100) sprintf(fan, " %d",fan_val);
    else sprintf(fan, "%d", fan_val);
  }
  else if (!(buttons & 0x01)) {
    if (fan_val == 00) fan_val = 255;
    else fan_val--;
    if (fan_val < 10) sprintf(fan, "  %d",fan_val);
    else if (fan_val < 100) sprintf(fan, " %d",fan_val);
    else sprintf(fan, "%d", fan_val);
  }
}

//This function is to convert the millis to a readable min:sec format
char conv_currtime_disp(unsigned long input_milli){
  unsigned long allSeconds=input_milli/1000;
  int secsRemaining=allSeconds%3600;
  int runMinutes=secsRemaining/60;
  int runSeconds=secsRemaining%60;
  if (roasting_started == 1) sprintf(roast_time_char,"%02d:%02d",runMinutes,runSeconds);
  else if (cooling_started == 1) sprintf(cool_time_char,"%02d:%02d",runMinutes,runSeconds);
}

void fan1_ramp_up(int step_size) {
  fan_val = 0;
  for (int i = 0; i <= 35; i++) {
    fan_val = fan_val+step_size;
    analogWrite(FAN_PIN, fan_val);
    delay(10); //To give time for the current to settle after step
  }
  sprintf(fan, "%d", fan_val);
}

ISR(TIMER1_COMPA_vect) {
  refresh_display_flag = 1;
  check_buttons_flag++;
}

/*
ISR(TIMER4_COMPA_vect) {
  ADC_sample_flag = 1;
}*/

//The state flags are set so the second time around it doesn't refresh the entire display
void refreshlcd(){
  if (refresh_display_flag == 1){
    if (CurrentState == state_idle) {
      display_idle();
      idle_state_flag = 1;
      Serial.print(F(",Ambient,"));
      Serial.print(thermocouple.readInternal());
      Serial.print(F(",Temp,"));
      Serial.println(Input);
      //Serial.print(F(",state,"));
      //Serial.println(roast_time_char);
      //Serial.print(Input);
    }
    else if (CurrentState == state_roasting) {
      //Serial.print("Refresh LCD,");
      Serial.print(F(",Ambient,"));
      Serial.print(thermocouple.readInternal());
      Serial.print(F(",Temp,"));
      Serial.print(Input);
      Serial.print(F(",roast value,"));
      Serial.println(roast);
      conv_currtime_disp(roast_time);
      display_roasting();
      roasting_state_flag = 1;
    }
    else if (CurrentState == state_cooling) {
      Serial.print(F(",Ambient,"));
      Serial.print(thermocouple.readInternal());
      Serial.print(F(",Temp,"));
      Serial.println(Input);
      //Serial.print(F(",state,"));
      //Serial.println(CurrentState);
      //Serial.print(Input);
      conv_currtime_disp(cool_time);
      display_cooling();
      cooling_state_flag = 1;
    }
    else if (CurrentState == state_debug) {
      display_debug();
      debug_state_flag = 1;
    }
    refresh_display_flag = 0;
  }
}

void display_idle() {
    if (idle_state_flag == 0) {
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

void display_roasting() {
  if (roasting_state_flag == 0){
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
  if (cooling_state_flag == 0){
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

void display_debug() {
  if (debug_state_flag == 0){
    lcd.lcdGoToXY(1,1);
    lcd.lcdWrite("Debug ");
    lcd.lcdGoToXY(7,1);
    lcd.lcdWrite("Temp = ");
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite("Fan ");
    lcd.lcdGoToXY(5,2);
    lcd.lcdWrite(fan);
    lcd.lcdGoToXY(8,2);
    lcd.lcdWrite("  SSR ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input_buffer);
    lcd.lcdGoToXY(5,2);
    lcd.lcdWrite(fan);
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
}

double get_temp(void) {
  if (!simulation){
    double c = thermocouple.readCelsius();
    if (isnan(c)) Input = Input;
    else Input = (c + Input)/2;
  }
  else Input = simulation_temp();
  return Input;
}
    
double simulation_temp(void) {
  if (CurrentState == 0) {
    count = 0;
    if ((Input_simulation > 0) || (Input_simulation < 78)) Input_simulation = 77;
    else Input_simulation = (Input_simulation - Input_simulation/9);
  }
    else {
    if (RELAY == 0) {
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
      if (linear) {
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
      else {
        Input_simulation = 0.0005*pow(count,3)-0.1257*pow(count,2)+11.012*count +51.339;
        count++;
        }
      }
    }
  return Input_simulation;
}
