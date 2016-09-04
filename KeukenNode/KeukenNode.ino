/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * REVISION HISTORY
 * Version 1.0 - Henrik EKblad
 * Version 1.5 - Johan van der Kuijl - send new temp at least every x minutes
 * 
 * DESCRIPTION
 * Measure temp and motion in the kitchen
 */

// Enable debug prints to serial monitor
#define MY_DEBUG 

// Enable and select radio type attached
#define MY_RADIO_NRF24
//#define MY_RADIO_RFM69

#define SKETCH_NAME "Keuken OTA"
#define SKETCH_MAJOR_VER "1"
#define SKETCH_MINOR_VER "5"

#include <SPI.h>
#include <MySensors.h>  
#include <DallasTemperature.h>
#include <OneWire.h>

// DALLAS
#define COMPARE_TEMP 1 // Send temperature only if changed? 1 = Yes 0 = No
#define ONE_WIRE_BUS 4 // Pin where dallase sensor is connected 
#define MAX_ATTACHED_DS18B20 16
OneWire oneWire(ONE_WIRE_BUS); // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
DallasTemperature sensors(&oneWire); // Pass the oneWire reference to Dallas Temperature. 
float lastTemperature[MAX_ATTACHED_DS18B20];
int numSensors=0;
boolean receivedConfig = false;
boolean metric = true; 

// Initialize temperature message
#define CHILD_ID_TEMP 0
MyMessage msg(CHILD_ID_TEMP,V_TEMP);

// MOTION
#define DIGITAL_INPUT_SENSOR 3 // Pin where motion sensor is connected (Only 2 and 3 generates interrupt!)
#define INTERRUPT DIGITAL_INPUT_SENSOR-2 // Usually the interrupt = pin -2 (on uno/nano anyway)
#define CHILD_ID_MOTION 10
MyMessage msgMotion(CHILD_ID_MOTION, V_TRIPPED);

unsigned long SLEEP_TIME = 30000; // Sleep time between reads (in milliseconds)
unsigned long FORCE_SEND_TIME = 120000; // 3600000 = 1 hour, 300000 = 5 minutes, 120000 = 2 minutes
unsigned long rolltime = millis() + FORCE_SEND_TIME; // track the rolling time with rolltime

void setup() 
{
  pinMode(DIGITAL_INPUT_SENSOR, INPUT);      // sets the motion sensor digital pin as input

  // Startup up the OneWire library
  sensors.begin();
  // requestTemperatures() will not block current thread
  sensors.setWaitForConversion(false);
}

void presentation()  {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo(SKETCH_NAME, SKETCH_MAJOR_VER "." SKETCH_MINOR_VER);

  // Fetch the number of attached temperature sensors  
  numSensors = sensors.getDeviceCount();

  if (numSensors == 0) {
    Serial.println("Error: no temp sensors found" );
  }
  
  // Present all sensors to controller
  for (int i=0; i<numSensors && i<MAX_ATTACHED_DS18B20; i++) {   
     present(i, S_TEMP);
  } 

  // Present the motionsensor
  present(CHILD_ID_MOTION, S_MOTION);    
  
}


void loop()      
{     
  // Read digital motion value
  boolean tripped = digitalRead(DIGITAL_INPUT_SENSOR) == HIGH; 
        
  send(msgMotion.set(tripped?"1":"0"));  // Send tripped value to gw   

  // Fetch temperatures from Dallas sensors
  sensors.requestTemperatures();

  // query conversion time and sleep until conversion completed
  int16_t conversionTime = sensors.millisToWaitForConversion(sensors.getResolution());
  // sleep() call can be replaced by wait() call if node need to process incoming messages (or if node is repeater)
  wait(conversionTime);

  // Read temperatures and send them to controller 
  for (int i=0; i<numSensors && i<MAX_ATTACHED_DS18B20; i++) {
 
    // Fetch and round temperature to one decimal
    float temperature = static_cast<float>(static_cast<int>((getConfig().isMetric?sensors.getTempCByIndex(i):sensors.getTempFByIndex(i)) * 10.)) / 10.;
 
    // Only send data if temperature has changed and no error
    #if COMPARE_TEMP == 1
    if (lastTemperature[i] != temperature && temperature != -127.00 && temperature != 85.00) {
    #else
    if (temperature != -127.00 && temperature != 85.00) {
    #endif
      // Send in the new temperature
      send(msg.setSensor(i).set(temperature,1));
      // Save new temperatures for next compare
      lastTemperature[i]=temperature;
    }

    // And send the temp each hour anyway
    if((long)(millis() - rolltime) >= 0) {
      send(msg.setSensor(i).set(temperature,1));
      lastTemperature[i]=temperature;
      // store new time  
      rolltime += FORCE_SEND_TIME;
    }    
  }

  if(isTransportOK()) {
    // Sleep until something happens, or after SLEEP_TIME
    sleep(INTERRUPT,CHANGE, SLEEP_TIME);  
  } else {
    wait(5000); // transport is not operational, allow the transport layer to fix this
  }  
}



