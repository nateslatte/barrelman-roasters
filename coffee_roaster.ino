/*************************************************** 
Pretty crappy coffee roasting code.  That is all

Written by Nathan Slattengren.  
BSD license, all text above must be included in any redistribution
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
#define FAN2_PIN 12
#define LED_GREEN_BOTTOM 8
#define LED_GREEN_MIDDLE 9
#define LED_RED_TOP 10


// initialize the Thermocouple
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);

//Define Variables we'll be connecting to
double Setpoint, Input, Output, temperature;

unsigned long  modelTime, serialTime;

//Run a fake temperature increase
boolean simulation = false;
boolean linear = false;
double Input_simulation = 75;

unsigned long windowStartTime;  

#if defined(ARDUINO_ARCH_SAMD)
// for Zero, output on USB Serial console, remove line below if using programming port to program the Zero!
 #define Serial SerialUSB
#endif

void setup() {
	Serial.begin(9600);
	Wire.begin();
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(FAN2_PIN, OUTPUT);
  pinMode(MAXCLK, OUTPUT);
  pinMode(LED_GREEN_BOTTOM, OUTPUT);
  pinMode(LED_GREEN_MIDDLE, OUTPUT);
  pinMode(LED_RED_TOP, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(FAN2_PIN, LOW);
  digitalWrite(LED_GREEN_MIDDLE, HIGH);
  digitalWrite(LED_GREEN_BOTTOM, HIGH);

	windowStartTime = millis();

	//Flash the start screen
	display_start();
  
	serialTime = 0;
	
	//wait for MAX chip to stabilize if simulation is not happening
	if (simulation) {
		delay(1000);  
		lcd.lcdClear(); 
	}
}

int roasting_started = 0;
int cooling_started = 0;
unsigned long roast_time_start = 0;
double count = 0;
int SaveState = 0;
int LEDBlink = 1;
int RELAY = 0;

void loop() {
  digitalWrite(LED_RED_TOP, LEDBlink);
  LEDBlink = !LEDBlink;
  
  Serial.print("State is: ");
  if (CurrentState == 0) Serial.println("idle");
  else if (CurrentState == 4) Serial.println("roast");
  else if (CurrentState == 6) Serial.println("cool");
  else Serial.println("transition");
  

  
	buttons = lcd.readButtons();
	Input = get_temp();
 
 Serial.println(Input);
	
	switch (CurrentState) {
		case state_idle:{
			roasting_started = 0; 
			cooling_started = 0;
			analogWrite(RELAY_PIN, 0);
      RELAY = 0;
      digitalWrite(FAN2_PIN, LOW);
      //digitalWrite(FAN2_PIN, LOW);
      analogWrite(FAN_PIN, 0);
    
			if (idle_state_flag == 0) idle_state_flag == 1;
  
			if(buttons & 0x01) CurrentState = state_idle;
			else CurrentState = state_idle_transition;
      
			if(!(buttons & 0x08)) {
        if (roast_val == 255) roast_val = 00;
        else roast_val++;
        Serial.print("Roast power: ");
        Serial.println(roast_val);
        //if (roast_val < 10) sprintf(roast, "  %d",roast_val);
        //else if (roast_val < 100) sprintf(roast, " %d",roast_val);
        //else sprintf(roast, "%d", roast_val);
			}
     else if (!(buttons & 0x04)) {
        if (roast_val == 00) roast_val = 255;
        else roast_val--;
        Serial.print("Roast power: ");
        Serial.println(roast_val);
        //if (roast_val < 10) sprintf(roast, "  %d",roast_val);
        //else if (roast_val < 100) sprintf(roast, " %d",roast_val);
        //else sprintf(roast, "%d", roast_val);
      }
     
			display_idle(Input, roast, idle_state_flag);
  
			if (idle_state_flag == 0) idle_state_flag = 1;
		}
		break;
    
		case state_idle_transition:
			lcd.lcdClear();
      CurrentState = state_roasting;
			idle_state_flag = 0;
      cooling_fan_ramp_up();
		break;
    
		case state_roasting: {
			unsigned long roast_time;
			unsigned long now = millis();
			if (roasting_started == 0) {
				roasting_started = 1;
				roast_time_start = now;
			}
			else roast_time = (now - roast_time_start)/1000;

      analogWrite(RELAY_PIN, roast_val);
      RELAY = 1;
      //digitalWrite(FAN2_PIN, HIGH);
      //analogWrite(FAN_PIN, 50);
      
      if(!(buttons & 0x08)) {
        if (roast_val == 255) roast_val = 00;
        else roast_val++;
        Serial.println(roast_val);
        //if (roast_val < 10) sprintf(roast, "  %d",roast_val);
        //else if (roast_val < 100) sprintf(roast, " %d",roast_val);
        //else sprintf(roast, "%d", roast_val);
      }
      else if (!(buttons & 0x04)) {
        if (roast_val == 00) roast_val = 255;
        else roast_val--;
        Serial.print("Roast power: ");
        Serial.println(roast_val);
        //if (roast_val < 10) sprintf(roast, "  %d",roast_val);
        //else if (roast_val < 100) sprintf(roast, " %d",roast_val);
        //else sprintf(roast, "%d", roast_val);
      }
			
			if (!(buttons & 0x02)) CurrentState = state_roasting_transition;
			else CurrentState = state_roasting;
  
			display_roasting(Input, roast, roast_time, roasting_state_flag);
  
			if (roasting_state_flag == 0) roasting_state_flag = 1;
		}
		break;

		case state_roasting_transition:
			roasting_state_flag = 0;
      roasting_started = 0;
			lcd.lcdClear();
			CurrentState = state_cooling;
		break;
   
    case state_cooling: {
      analogWrite(RELAY_PIN, 0);
      RELAY = 0;
      //digitalWrite(FAN2_PIN, HIGH);
      analogWrite(FAN_PIN, 50);
      Setpoint = 125;

      unsigned long now = millis();
      unsigned long roast_time;
      if (cooling_started == 0) {
        cooling_started = 1;
        roast_time_start = now;
      }
      else{
        roast_time = (now - roast_time_start)/1000;
      }
      

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
      
      display_cooling(Input, roast, roast_time, cooling_state_flag);
      if (cooling_state_flag == 0) cooling_state_flag = 1;
    } 
    break;

    case state_cooling_transition:
      lcd.lcdClear();
      cooling_started = 0;
      cooling_state_flag = 0;
      CurrentState = state_idle;
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

void cooling_fan_ramp_up() {
  int fan_level = 0;
  for (int i = 0; i <= 30; i++) {
    fan_level = fan_level+5;
    analogWrite(FAN_PIN, fan_level);
    delay(50);
  }
}

void display_idle(double temp, char* roast_type, int active) {
    if (active == 0) {
		lcd.lcdGoToXY(1,1);
		lcd.lcdWrite("Idle  ");
		lcd.lcdGoToXY(7,1);
		lcd.lcdWrite("Temp = ");
		lcd.lcdGoToXY(14,1);
		lcd.lcdWrite(temp);
		lcd.lcdGoToXY(1,2);
		lcd.lcdWrite("Start? ");
		lcd.lcdGoToXY(8,2);
		lcd.lcdWrite("PWR = ");
		lcd.lcdGoToXY(14,2);
		lcd.lcdWrite(roast_type);
	}
	else {
		lcd.lcdGoToXY(14,1);
		lcd.lcdWrite(temp);
		lcd.lcdGoToXY(14,2);
		lcd.lcdWrite(roast_type);
	}
}



void display_roasting(double temp, char* roast_type, unsigned long roast_time, int active) {
  if (active == 0){
    lcd.lcdGoToXY(1,1);
    lcd.lcdWrite("Roast ");
		lcd.lcdGoToXY(7,1);
		lcd.lcdWrite("Temp = ");
		lcd.lcdGoToXY(14,1);
		lcd.lcdWrite(temp);
		lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time);
		lcd.lcdGoToXY(7,2);
		lcd.lcdWrite("Stop?");
    lcd.lcdGoToXY(12,2);
    lcd.lcdWrite("  ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite(roast_type);
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(temp);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time);
  }
}

void display_cooling(double temp, char* roast_type, unsigned long roast_time, int active) {
  if (active == 0){
    lcd.lcdGoToXY(1,1);
    lcd.lcdWrite("Roast ");
    lcd.lcdGoToXY(7,1);
    lcd.lcdWrite("Temp = ");
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(temp);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time);
    lcd.lcdGoToXY(7,2);
    lcd.lcdWrite("Stop?");
    lcd.lcdGoToXY(12,2);
    lcd.lcdWrite("  ");
    lcd.lcdGoToXY(14,2);
    lcd.lcdWrite("  0");
  }
  else {
    lcd.lcdGoToXY(14,1);
    lcd.lcdWrite(temp);
    lcd.lcdGoToXY(1,2);
    lcd.lcdWrite(roast_time);
  }
}

double get_temp(void) {
	if (!simulation){
		Serial.print("Temp: ");
    Serial.println(thermocouple.readInternal());
		Input = thermocouple.readFarenheit();
		//Serial.println(temperature);
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
			if (Input_simulation > 77 && Input_simulation < 100 ) Input_simulation = Input_simulation-0.089;
			else if (Input_simulation > 100 && Input_simulation < 125 ) Input_simulation = Input_simulation-0.128;
			else if (Input_simulation > 125 && Input_simulation < 150 ) Input_simulation = Input_simulation-0.321;
			else if (Input_simulation > 150 && Input_simulation < 200 ) Input_simulation = Input_simulation-0.81;
			else if (Input_simulation > 200 && Input_simulation < 250 ) Input_simulation = Input_simulation-1.2;
			else if (Input_simulation > 250 && Input_simulation < 300 ) Input_simulation = Input_simulation-1.4;
			else if (Input_simulation > 300 && Input_simulation < 350 ) Input_simulation = Input_simulation-1.6;
			else if (Input_simulation > 350 && Input_simulation < 400 ) Input_simulation = Input_simulation-1.8;
			else if (Input_simulation > 400 && Input_simulation < 450 ) Input_simulation = Input_simulation-2;
			else if (Input_simulation > 450 && Input_simulation < 500 ) Input_simulation = Input_simulation-2.2;
			else Input_simulation = Input_simulation;
		}
		else {
			if (linear) {
				if (Input_simulation < 100) Input_simulation = Input_simulation + 2.4;
				else if (Input_simulation > 100 && Input_simulation < 150) Input_simulation = Input_simulation + 2.2;
				else if (Input_simulation > 150 && Input_simulation < 200) Input_simulation = Input_simulation + 1.8;
				else if (Input_simulation > 200 && Input_simulation < 250) Input_simulation = Input_simulation + 1.4;
				else if (Input_simulation > 250 && Input_simulation < 300) Input_simulation = Input_simulation + 0.81;
				else if (Input_simulation > 300 && Input_simulation < 350) Input_simulation = Input_simulation + 0.61;
				else if (Input_simulation > 350 && Input_simulation < 375) Input_simulation = Input_simulation + 0.321;
				else if (Input_simulation > 375 && Input_simulation < 400) Input_simulation = Input_simulation + 0.128;
				else if (Input_simulation > 400 && Input_simulation < 425) Input_simulation = Input_simulation + 0.089;
				else if (Input_simulation > 425) Input_simulation = Input_simulation + .0357;
			}
			else {
				Input_simulation = 0.0005*pow(count,3)-0.1257*pow(count,2)+11.012*count +51.339;
				count++;
			}
		}
    }
	return Input_simulation;
}
