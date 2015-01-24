//
// Capacitive touch light switch
//
#include <Arduino.h>
#include <avr/wdt.h>
#include <EEPROM.h>
#include "LowPower.h"
#include "TouchSequence.h"
#include "RFM12B.h"
#include "SwitchProtocol.h"
#include "SwitchSettings.h"
#include "util.h"

#ifndef NETWORKID
#error "NETWORKID must be defined!"
#endif

static const int mpr121Addr         = 0x5A;
static const int mpr121IntPin       = 1;    // int 1 == pin 3

static MPR121Settings mpr121Settings;
static RFM12BSettings rfm12bSettings;
static SleepSettings  sleepSettings;

RFM12B radio;
TouchSequence touch(mpr121Addr, mpr121IntPin);
period_t sleepPeriod = SLEEP_FOREVER;

extern long readVcc();

void sleep(period_t time) {
    Serial.print("sleep: ");
    Serial.println(time);

    // flush serial for debugging before powering down
    Serial.flush();
    delay(100);

    // turn off the wdt in case it was previously set...this is to prevent a
    // spurious wakeup after an interrupt and then sleep forever call.
    if (time == SLEEP_FOREVER) {
        wdt_disable();
        LowPower.powerDown(time, ADC_OFF, BOD_OFF);
    }
    else
        LowPower.powerStandby(time, ADC_OFF, BOD_OFF);
}

void handleReply() {
    Serial.println("handleReply: ");
    if (*radio.DataLen == 0)
        return;
    unsigned char type = *(unsigned char *)radio.Data;
    switch (type) {
        case SwitchPacket::PING:
            Serial.println("ping");
            break;
        case SwitchPacket::CONFIGURE:
            Serial.println("configure");
            break;
        case SwitchPacket::RESET:
            Serial.println("reset");
            break;
        default:
            Serial.print("Unknown type: ");
            Serial.println(type);
            break;
    }
}

void waitForReply(bool sleep = true) {
    long now = millis();
    while (millis() - now <= sleepSettings.replyWakeLock) {
        if (radio.ACKReceived(GATEWAYID)) {
            handleReply();
            break;
        }
    }
    if (sleep)
        radio.Sleep(sleepSettings.statusInterval, sleepSettings.statusScaler);
}

/* Sends a touch event to the base station */
void handleEvent(byte repeated) {
    TouchEvent pkt;
    pkt.gesture = touch.getGesture();
    pkt.electrode = touch.getLastTouch();
    pkt.repeat = repeated;
    switch (pkt.gesture) {
        case TOUCH_SWIPE_DOWN:  Serial.println("swipe down");   break;
        case TOUCH_SWIPE_UP:    Serial.println("swipe up");     break;
        case TOUCH_SWIPE_LEFT:  Serial.println("swipe left");   break;
        case TOUCH_SWIPE_RIGHT: Serial.println("swipe right");  break;
        case TOUCH_PROXIMITY:   Serial.println("proximity");    break;
        case TOUCH_TAP:
            Serial.print("tap ");
            Serial.println(pkt.electrode);
            break;
        case TOUCH_DOUBLE_TAP:
            Serial.print("double tap ");
            Serial.println(pkt.electrode);
            break;
        default:
            Serial.println("no gesture");
            return;
    }
    radio.Wakeup();
    radio.Send(GATEWAYID, (const void*)(&pkt), sizeof(pkt), !repeated);
    if (!repeated)
        waitForReply();
}

void sendStatus() {
    SwitchStatus pkt;
    pkt.batteryLevel = readVcc();
    Serial.print("vcc: ");
    Serial.println(pkt.batteryLevel);

    radio.Wakeup();
    radio.Send(GATEWAYID, (const void*)(&pkt), sizeof(pkt), false);
    radio.Sleep(sleepSettings.statusInterval, sleepSettings.statusScaler);
}

void loadConfiguration() {
    int offset = 0;
    if (EEPROM.read(offset++) != FIRMWARE_VERSION)
        return;

    // second byte bitmask indicating which settings are set
    byte enabled = EEPROM.read(offset++);
    if (enabled & SETTINGS_MPR121)
        loadSettings(offset, mpr121Settings);
    offset += sizeof(MPR121Settings);

    if (enabled & SETTINGS_RFM12B)
        loadSettings(offset, rfm12bSettings);
    offset += sizeof(RFM12BSettings);

    if (enabled & SETTINGS_SLEEP)
        loadSettings(offset, sleepSettings);
    offset += sizeof(SleepSettings);
}

void setup() {
    // initialize serial
    Serial.begin(115200);
    while (!Serial);
    Serial.println("initializing...");

    // D5 GND for electrodes
    //pinMode(5, OUTPUT);
    //digitalWrite(5, LOW);

    loadConfiguration();

    Serial.println("  * radio...");
    radio.Initialize(rfm12bSettings.nodeId, DEFAULT_FREQ_BAND, NETWORKID);
    /* radio.Encrypt(KEY); */
    radio.Sleep(sleepSettings.statusInterval, sleepSettings.statusScaler);

    Serial.println("  * touch sensor...");
    touch.begin(mpr121Settings);

    // proximity thresholds
    touch.setTouchThreshold(2, 12);
    touch.setReleaseThreshold(1, 12);

    touch.enableInterrupt();
    touch.sleep();
    touch.dump();
}

void loop() {
    if (!touch.isInterrupted())
        sleep(sleepPeriod);

    if (radio.DidTimeOut())
        sendStatus();

    if (touch.isInterrupted()) {
        // either a touch or release event woke us up
        touch.update();
        if (touch.isTouched())
            sleepPeriod = (period_t)sleepSettings.touch;
        else if (touch.isProximity())
            sleepPeriod = (period_t)sleepSettings.proximity;
        else
            sleepPeriod = (period_t)sleepSettings.release;
        touch.wakeUp();
        touch.enableInterrupt();
    }
    else if (touch.isTouched() || touch.isProximity()) {
        // we woke up w/o an interrupt, but an electrode or proximity sensor
        // is still being touched - this is a repeat event
        touch.update();
        Serial.println("repeat:");
        handleEvent(1);
        sleepPeriod = (period_t)sleepSettings.repeat;
    }
    else {
        // no touch interrupt, no current touch - this is a timeout.
        // handle the event and then clear everything to reset.
        handleEvent(0);
        touch.clear();
        touch.sleep();
        Serial.println("touch done:");
        Serial.println();
        sleepPeriod = SLEEP_FOREVER;
    }
}
