/*
 * Sketch to read pulses from pulse meter; 
 * Only accepts a pulse change if the signal has been stable for more than x seconds;
 * Only sends message to gateway after more than x seconds have passed since sending last message;
 * Pulses are assumed to equal 1 liter of water
 * 
 * Author  Sandor Incze & Michel Schilthuizen
 * Created 2016-05-29
 * Version 1.0
 *
 * Based on the flow reader sketch "WaterMeterPulseSensor" by 
 * Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 * 
 * During testing in can be handy to be able to set the values in domoticz; You can do this by using the REST request below:
 * (replace x.x.x.x and deviceid by relevant numbers
 * http://x.x.x.x:8080/json.htm?type=command&param=udevice&idx=deviceid&nvalue=0&svalue=0
 */

// Enable debug prints to serial monitor
//#define MY_DEBUG 

// Enable and select radio type attached
#define MY_RADIO_NRF24

#include <SPI.h>
#include <MySensors.h>  
#define IN_PIN 3                     // PIN NUMBER OF DIGITAL OUTPUT TCRT5000
#define INTERRUPT_DEBOUNCE_DELAY 300 // debounce in milliseconds            
#define CHILD_ID 1                   // Id of the sensor child
#define SKETCH_NAME "Meterkast OTA"
#define SKETCH_MAJOR_VER "1"
#define SKETCH_MINOR_VER "1"

// Gasmeter
// G4 J.B. Rombach RF1 1992
// Qmax 6m3/h     = 100 l/min
// Qmin 0,04 m3/h = 0,67 l/min

unsigned long SEND_FREQUENCY = 5000;           // Minimum time between send (in milliseconds). We don't want to spam the gateway.

MyMessage flowMsg(CHILD_ID,V_FLOW);
MyMessage volumeMsg(CHILD_ID,V_VOLUME);
MyMessage lastCounterMsg(CHILD_ID,V_VAR1);

volatile double flow = 0;  
boolean gw_received = false;
boolean seeninterrupt = false;

double flowSent =0;                     

unsigned long globalCounter  =0;
unsigned long old_globalCounter =0;
unsigned long lastSent =0;
unsigned long lastPulse =0;

static volatile int last_pin_value = 0;
static volatile int stable_pin_value = 0;
static volatile int last_stable_pin_value = 0;
static unsigned long lastInterrupt = 0;

void receive(const MyMessage &message) {
  Serial.println("--------------- function receive ---------------"); 
  unsigned long gwPulseCount = 0;
  if (message.type==V_VAR1) {
    gwPulseCount=message.getULong();
    if (globalCounter != gwPulseCount) 
    {
      globalCounter += gwPulseCount;
      Serial.print("Received last pulse count from gw: ");
      Serial.println(globalCounter);
      //use line below to reset your counter in domoticz; We needed it ;-)
      //globalCounter = 0;
      gw_received = true;
      old_globalCounter = globalCounter;
    }
  }
  Serial.println("---------------------------------------------------");
} 

void setup() {
  lastInterrupt = millis();
}

void presentation()  {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo(SKETCH_NAME, SKETCH_MAJOR_VER "." SKETCH_MINOR_VER);

  // Register this device as Gasflow sensor
  present(CHILD_ID, S_GAS); 
}

void loop() {
  if (!gw_received) {
      //Last Pulsecount not yet received from controller, request it again
      Serial.println("Requesting as nothing was received from gateway yet!");
      request(CHILD_ID, V_VAR1);
      wait(1000);
      return;
  }
  else 
  {
    int pin_value = digitalRead(IN_PIN);
  
    // Calculate time passed since last interrupt
    unsigned long timedifference = millis() - lastInterrupt;
  
    // In case millis cycled to 0 (as it goes beyong max unsigned long) reset values
    if (timedifference < 0)
    {
      lastInterrupt = 0;
      timedifference = 0;
      lastSent = 0;
    }
   
    if (pin_value != last_pin_value) //if the pin changes from high to low or low to high
    {
      Serial.println("Pin value changed!");
      lastInterrupt=millis();
      last_pin_value = pin_value;
    }
    else if ((stable_pin_value != pin_value) && (timedifference >= (INTERRUPT_DEBOUNCE_DELAY ))) //if stable pin is different from the pin and more than debounce delay has passed, switch stable pin
    {
      stable_pin_value = pin_value;
      Serial.println("Stable pin value different!");
    }
  
    if ((stable_pin_value != last_stable_pin_value) && (stable_pin_value == HIGH))  
    {// Only raise counter if filtered pin value goes from 0 to 1
      ++globalCounter;
      seeninterrupt = true;
      Serial.print("Real Interrupt NOW; Global counter is now: "); //  Debug Information will show the interrupt we will count.
      Serial.println(globalCounter);
    }

    //reset last_stable_pin_value also when it has changed to LOW
    if (stable_pin_value != last_stable_pin_value)
    {
      last_stable_pin_value = stable_pin_value;
    }

    //if we have seen an interrupt and the send frequency has passed, send message to gateway; 
    //Also send a message if last message has been sent more than x seconds ago and flow is still more than 0
    if ( (gw_received) && (millis() - lastSent > SEND_FREQUENCY) 
      && (seeninterrupt || flowSent > 0))
    {
      double liters = globalCounter - old_globalCounter;
      float minutes_passed = 1.0 * (millis()- lastSent) / 60000.0;
      double liters_per_minute = 1.0 * liters / minutes_passed; 
      flowSent = liters_per_minute;
      
      Serial.print("Minutes Passed: ");
      Serial.println(minutes_passed);
      Serial.print("Aantal Liters: ");
      Serial.println(liters);
      Serial.print("Liters per Minuut: ");
      Serial.println(liters_per_minute);
  
      send(lastCounterMsg.set(globalCounter));                  // Send  globalcounter value to gw in VAR1
      send(flowMsg.set(liters_per_minute, 2));                  // Send flow value to gw
      send(volumeMsg.set(1.0 * globalCounter / 1000, 3));       // Send volume value to gw and convert from dm3 to m3
          
      seeninterrupt = false;
      lastSent = millis();
      old_globalCounter = globalCounter;
    }
  }
}
