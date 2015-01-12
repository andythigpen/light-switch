//
// Capacitive touch light switch
//
#include <avr/wdt.h>
#include "LowPower.h"
#include "TouchSequence.h"

static const int mpr121Addr   = 0x5A;
static const int mpr121IntPin = 1;      // int 1 == pin 3

TouchSequence touch(mpr121Addr, mpr121IntPin);
period_t sleepPeriod = SLEEP_FOREVER;

void setup() {
    // initialize serial
    Serial.begin(57600);
    while (!Serial);
    Serial.println("initializing...");

    touch.begin(5, 2);
}

void sleep(period_t time) {
    touch.enableInterrupt();

    // flush serial for debugging before powering down
    Serial.flush();

    // turn off the wdt in case it was previously set...this is to prevent a
    // spurious wakeup after an interrupt and then sleep forever call.
    if (time == SLEEP_FOREVER)
        wdt_disable();
    LowPower.powerDown(time, ADC_OFF, BOD_OFF);
}

void handleEvent() {
    switch (touch.getGesture()) {
        case TOUCH_SWIPE_DOWN:
            Serial.println("swipe down");
            break;
        case TOUCH_SWIPE_UP:
            Serial.println("swipe up");
            break;
        case TOUCH_SWIPE_LEFT:
            Serial.println("swipe left");
            break;
        case TOUCH_SWIPE_RIGHT:
            Serial.println("swipe right");
            break;
        case TOUCH_TAP:
            Serial.print("tap ");
            Serial.println(touch.getLastTouch());
            break;
        case TOUCH_DOUBLE_TAP:
            Serial.print("double tap ");
            Serial.println(touch.getLastTouch());
            break;
        case TOUCH_PROXIMITY:
            Serial.println("proximity");
            break;
        default:
            Serial.println("no gesture");
            break;
    }
}

void loop() {
    sleep(sleepPeriod);

    if (touch.isInterrupted()) {
        // either a touch or release event woke us up
        if (touch.update()) {
            Serial.print("touch event: ");
            Serial.println(touch.getLastTouch());
            sleepPeriod = SLEEP_500MS;
        }
        else if (touch.isProximity()) {
            Serial.println("proximity event: ");
            sleepPeriod = SLEEP_1S;
        }
        else {
            Serial.print("release event: ");
            Serial.println(touch.getLastTouch());
            sleepPeriod = SLEEP_250MS;
        }
    }
    else if (touch.isTouched() || touch.isProximity()) {
        // we woke up w/o an interrupt, but there was a previous
        // touch - this is a repeat event
        touch.update();
        Serial.println("repeat");
        handleEvent();
        sleepPeriod = SLEEP_250MS;
    }
    else {
        // no touch interrupt, no previous touch...handle the event and then
        // clear everything to reset.
        handleEvent();
        touch.clear();
        Serial.println("Sleeping forever");
        Serial.println();
        sleepPeriod = SLEEP_FOREVER;
    }
}
