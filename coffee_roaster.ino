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
int buttons;

// Default connection is using software SPI, but comment and uncomment one of
// the two examples below to switch between software SPI and hardware SPI:

// Example creating a thermocouple instance with software SPI on any three
// digital IO pins.
#define MAXDO   0
#define MAXCS   1
#define MAXCLK 30 // The TX LED has a defined Arduino pin
#define RELAY_PIN 6
#define FAN1_PIN 11
#define FAN2_PIN 5
#define LED_GREEN_BOTTOM 8
#define LED_GREEN_MIDDLE 9
#define LED_RED_TOP 10


// initialize the Thermocouple
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);

//Define Variables we'll be connecting to
double Setpoint, Input, Output, temperature;

//Run a fake temperature increase
boolean simulation = false;
boolean linear = true;
double Input_simulation = 75;

unsigned long windowStartTime;  

#if defined(ARDUINO_ARCH_SAMD)
// for Zero, output on USB Serial console, remove line below if using programming port to program the Zero!
 #define Serial SerialUSB
#endif

void setup() {
  windowStartTime = millis();
  Wire.begin();
  Serial.begin(19200);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAN1_PIN, OUTPUT);
  pinMode(FAN2_PIN, OUTPUT);
  pinMode(MAXCLK, OUTPUT);
  pinMode(LED_GREEN_BOTTOM, OUTPUT);
  pinMode(LED_GREEN_MIDDLE, OUTPUT);
  pinMode(LED_RED_TOP, OUTPUT);

  analogWrite(RELAY_PIN, 0);
  analogWrite(FAN1_PIN, fan1_val);
  analogWrite(FAN2_PIN, fan2_val);
  digitalWrite(LED_GREEN_MIDDLE, HIGH);
  digitalWrite(LED_GREEN_BOTTOM, HIGH);

  //Flash the start screen
  display_start();

  // initialize timer1 
  noInterrupts();           // disable all interrupts
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;
  OCR1A = 15625;            // compare match register 16MHz/256/4Hz
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();  //allows interrupts
	
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

void loop() {
  digitalWrite(LED_RED_TOP, LEDBlink);
  LEDBlink = !LEDBlink;
  analogWrite(FAN2_PIN, fan2_val);
  if (check_buttons_flag >= 1){
    buttons = lcd.readButtons();
    check_buttons_flag = 0;
  }
  else buttons = 15;
  
  Input = get_temp();

  //Go ahead and write values to LCD
  refreshlcd();
  unsigned long now = millis();
  
	switch (CurrentState) {
		case state_idle:{
			analogWrite(RELAY_PIN, 0);
      RELAY = 0;
      fan2_val = 0;
      analogWrite(FAN1_PIN, fan1_val);
      
  
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
      fan1_ramp_up(4);
		}
		break;

    case state_idle_to_debug_transition:{
      lcd.lcdClear();
      CurrentState = state_debug;
      idle_state_flag = 0;
      char roast[4] = "00";
      int roast_val = 0;
      int fan1_val = 0;
      fan1_ramp_up(4);
    }
    break;
    
		case state_roasting: {
			if (roasting_started == 0) {
				roasting_started = 1;
				time_start = now;
	  		}
			else roast_time = (now - time_start)/1000;

      analogWrite(RELAY_PIN, roast_val);
      RELAY = 1;
      fan2_val = 255;
      
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
      analogWrite(FAN1_PIN, 150);
      Setpoint = 40;

      if (cooling_started == 0) {
        cooling_started = 1;
        time_start = now;
//        format_time();
      }
      else{
        //format_time();
        cool_time = (now - time_start)/1000;
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
      fan1_val = 0;
      cool_time = 0;
    break;

    case state_debug: {
      analogWrite(RELAY_PIN, roast_val);
      RELAY = 1;
      analogWrite(FAN1_PIN, fan1_val);

      if(buttons == 12) CurrentState = state_debug_transition;
      else CurrentState = state_debug;

      thermal_power_button();
      fan1_button();
    }
    break;

    case state_debug_transition:
      lcd.lcdClear();
      debug_state_flag = 0;
      CurrentState = state_cooling;
    break;
	}
}

//void format_time(int start_time){
//if (CurrentState == state_idle) {
//time_minute = 0;
//time_minute_text = "00";}
//else if (CurrentState == state_roasting){
  
//}
//milli()/1000 = seconds
//seconds/60 = minutes
//Seconds%60 = remaining seconds
//}

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

void fan1_button(){
  if(!(buttons & 0x02)) {
    if (fan1_val == 255) fan1_val = 00;
    else fan1_val++;
    if (fan1_val < 10) sprintf(fan1, "  %d",fan1_val);
    else if (fan1_val < 100) sprintf(fan1, " %d",fan1_val);
    else sprintf(fan1, "%d", fan1_val);
  }
  else if (!(buttons & 0x01)) {
    if (fan1_val == 00) fan1_val = 255;
    else fan1_val--;
    if (fan1_val < 10) sprintf(fan1, "  %d",fan1_val);
    else if (fan1_val < 100) sprintf(fan1, " %d",fan1_val);
    else sprintf(fan1, "%d", fan1_val);
  }
}

void fan1_ramp_up(int step_size) {
  fan1_val = 0;
  for (int i = 0; i <= 35; i++) {
    fan1_val = fan1_val+step_size;
    analogWrite(FAN1_PIN, fan1_val);
    delay(10); //To give time for the current to settle after step
  }
  sprintf(fan1, "%d", fan1_val);
}

ISR(TIMER1_COMPA_vect) {
  refresh_display_flag = 1;
  check_buttons_flag++;
}

//The state flags are set so the second time around it doesn't refresh the entire display
void refreshlcd(){
  if (refresh_display_flag == 1){
    if (CurrentState == state_idle) {
      display_idle();
      idle_state_flag = 1;
      Serial.print(F(",Ambient,"));
      Serial.print(thermocouple.readInternal());
      Serial.print(F(",Temp,"));
      Serial.print(Input);
      Serial.print(F(",state,"));
      Serial.println(CurrentState);
    }
    else if (CurrentState == state_roasting) {
      //Serial.print("Refresh LCD,");
      Serial.print(F(",Ambient,"));
      Serial.print(thermocouple.readInternal());
      Serial.print(F(",Temp,"));
      Serial.print(Input);
      Serial.print(F(",state,"));
      Serial.println(CurrentState);
      display_roasting();
      roasting_state_flag = 1;
    }
    else if (CurrentState == state_cooling) {
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
		lcd.lcdWrite(Input);
		lcd.lcdGoToXY(1,2);
		lcd.lcdWrite("Start? ");
		lcd.lcdGoToXY(8,2);
		lcd.lcdWrite("PWR = ");
		lcd.lcdGoToXY(14,2);
		lcd.lcdWrite(roast);
	}
	else {
		lcd.lcdGoToXY(14,1);
		lcd.lcdWrite(Input);
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
		lcd.lcdWrite(Input);
		lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time);
		lcd.lcdGoToXY(7,2);
		lcd.lcdWrite("Stop?");
    lcd.lcdGoToXY(12,2);
    lcd.lcdWrite("  ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time);
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
    lcd.lcdWrite(Input);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(cool_time);
    lcd.lcdGoToXY(7,2);
    lcd.lcdWrite("Stop?");
    lcd.lcdGoToXY(12,2);
    lcd.lcdWrite("  ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(cool_time);
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
    lcd.lcdWrite(Input);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite("Fan ");
    lcd.lcdGoToXY(5,2);
    lcd.lcdWrite(fan1);
    lcd.lcdGoToXY(8,2);
    lcd.lcdWrite("  SSR ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(Input);
    lcd.lcdGoToXY(5,2);
    lcd.lcdWrite(fan1);
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast);
  }
}

double get_temp(void) {
	if (!simulation){
		Input = thermocouple.readCelsius();
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
			if (Input_simulation > 77 && Input_simulation < 100 )       Input_simulation = Input_simulation-0.00089;
			else if (Input_simulation > 100 && Input_simulation < 125 ) Input_simulation = Input_simulation-0.00128;
			else if (Input_simulation > 125 && Input_simulation < 150 ) Input_simulation = Input_simulation-0.00321;
			else if (Input_simulation > 150 && Input_simulation < 200 ) Input_simulation = Input_simulation-0.0081;
			else if (Input_simulation > 200 && Input_simulation < 250 ) Input_simulation = Input_simulation-0.012;
			else if (Input_simulation > 250 && Input_simulation < 300 ) Input_simulation = Input_simulation-0.014;
			else if (Input_simulation > 300 && Input_simulation < 350 ) Input_simulation = Input_simulation-0.016;
			else if (Input_simulation > 350 && Input_simulation < 400 ) Input_simulation = Input_simulation-0.018;
			else if (Input_simulation > 400 && Input_simulation < 450 ) Input_simulation = Input_simulation-0.02;
			else if (Input_simulation > 450 && Input_simulation < 500 ) Input_simulation = Input_simulation-0.022;
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
