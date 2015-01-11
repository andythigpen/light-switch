//
// Capacitive touch light switch
//
#include <MPR121.h>
#include <Wire.h>
#include <LowPower.h>

#define BAUD_RATE 57600

void setup() {
    // initialize serial
    Serial.begin(BAUD_RATE);
    while (!Serial);
}

void loop() {
    //TODO
}
