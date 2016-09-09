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
 * Version 1.0 - Johan van der Kuijl
 * 
 * DESCRIPTION
 * This node resides in the garden and acts as repeater between house and garage.
 * It also:
 * - detects motion with HC-SR501 ir motion sensor (not implemented yet)
 * - measures temperature and humidity with DHT22 sensor (not implemented yet)
 * - controls a relay to switch on light 
 * 
 */
#define SKETCH_NAME "Afdak OTA"
#define SKETCH_MAJOR_VER "1"
#define SKETCH_MINOR_VER "8"

// Enable debug prints to serial monitor
//#define MY_DEBUG 

// Enable and select radio type attached
#define MY_RADIO_NRF24

// Use low power
#define MY_RF24_PA_LEVEL RF24_PA_LOW 

// Enabled repeater feature for this node
#define MY_REPEATER_FEATURE

#include <SPI.h>
#include <MySensors.h>

#define MOTION_PIN 2 // the motion sensor
#define CHILD_ID_MOTION 10
MyMessage msgMotion(CHILD_ID_MOTION, V_TRIPPED);

#define RELAY_1  3  // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define NUMBER_OF_RELAYS 1 // Total number of attached relays
#define RELAY_ON 1  // GPIO value to write to turn on attached relay
#define RELAY_OFF 0 // GPIO value to write to turn off attached relay

void before() { 
  for (int sensor=1, pin=RELAY_1; sensor<=NUMBER_OF_RELAYS;sensor++, pin++) {
    // Then set relay pins in output mode
    pinMode(pin, OUTPUT);   
  }
}

void setup() {
   pinMode(MOTION_PIN, INPUT);      // sets the motion sensor digital pin as input
}

void presentation()  
{  
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo(SKETCH_NAME, SKETCH_MAJOR_VER "." SKETCH_MINOR_VER);

  for (int sensor=1, pin=RELAY_1; sensor<=NUMBER_OF_RELAYS;sensor++, pin++) {
    // Register all sensors to gw (they will be created as child devices)
    present(sensor, S_LIGHT);
  }  

  // Present the motionsensor
  present(CHILD_ID_MOTION, S_MOTION);    

}

void loop() 
{
  uint8_t value;
  static uint8_t sentValue=2;
    
  // Read digital motion value
  value = digitalRead(MOTION_PIN); 

  if (value != sentValue) {
     // Value has changed from last transmission, send the updated value
     send(msgMotion.set(value==HIGH ? 1 : 0));
     sentValue = value;
  }  

  if(isTransportOK()) {
    // good.
  } else {
    wait(5000); // transport is not operational, allow the transport layer to fix this
  }  
}

void receive(const MyMessage &message) {
  // We only expect one type of message from controller. But we better check anyway.
  if (message.type==V_LIGHT) {
     // Change relay state
     digitalWrite(message.sensor-1+RELAY_1, message.getBool()?RELAY_ON:RELAY_OFF);

     // Write some debug info
     Serial.print("Incoming change for sensor:");
     Serial.print(message.sensor);
     Serial.print(", New status: ");
     Serial.println(message.getBool());
   } 
}

