//
// Capacitive touch light switch
//
#include <avr/wdt.h>
#include "LowPower.h"
#include "TouchSequence.h"
#include "RFM12B.h"

#define NODEID      2
#define NETWORKID   1
#define GATEWAYID   1

static const int mpr121Addr   = 0x5A;
static const int mpr121IntPin = 1;      // int 1 == pin 3

RFM12B radio;
TouchSequence touch(mpr121Addr, mpr121IntPin);
period_t sleepPeriod = SLEEP_FOREVER;

void setup() {
    // initialize serial
    Serial.begin(115200);
    while (!Serial);
    Serial.println("initializing...");

    radio.Initialize(NODEID, RF12_915MHZ, NETWORKID);
    /* radio.Encrypt(KEY); */
    radio.Sleep(); // sleep right away to save power

    touch.begin(5, 2);
    touch.sleep(SLEEP_ELECTRODES_OFF);
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

/* Sends a touch event to the base station */
void handleEvent(byte repeated) {
    TouchEvent pkt;
    pkt.gesture = touch.getGesture();
    pkt.electrode = touch.getLastTouch();
    pkt.repeat = repeated;
    switch (pkt.gesture) {
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
            Serial.println(pkt.electrode);
            break;
        case TOUCH_DOUBLE_TAP:
            Serial.print("double tap ");
            Serial.println(pkt.electrode);
            break;
        case TOUCH_PROXIMITY:
            Serial.println("proximity");
            break;
        default:
            Serial.println("no gesture");
            return;
    }
    radio.Wakeup();
    radio.Send(GATEWAYID, (const void*)(&pkt), sizeof(pkt), false);
    radio.Sleep();
}

void loop() {
    sleep(sleepPeriod);

    if (touch.isInterrupted()) {
        // either a touch or release event woke us up
        touch.update();
        if (touch.isTouched()) {
            Serial.print("touch event: ");
            Serial.println(touch.getLastTouch());
            sleepPeriod = SLEEP_500MS;
        }
        else if (touch.isProximity()) {
            Serial.println("proximity event: ");
            sleepPeriod = SLEEP_2S;
            // if (touch.wasAsleep()) {
                touch.wakeUp();
            // }
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
        handleEvent(1);
        sleepPeriod = SLEEP_250MS;
    }
    else {
        // no touch interrupt, no previous touch...handle the event and then
        // clear everything to reset.
        handleEvent(0);
        touch.clear();
        touch.sleep(SLEEP_ELECTRODES_OFF);
        Serial.println("Sleeping forever");
        Serial.println();
        sleepPeriod = SLEEP_FOREVER;
    }
}
