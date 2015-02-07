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

static SwitchSettings cfg;

RFM12B radio;
TouchSequence touch(mpr121Addr, mpr121IntPin);
period_t sleepPeriod = SLEEP_FOREVER;

extern long readVcc();

void softReset() {
    Serial.println("softReset:");
    Serial.flush();
    asm volatile ("  jmp 0");
}

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

void loadConfiguration() {
    Serial.println("loadConfiguration:");

    SwitchSettings current;
    EEPROM_readAnything(0, current);
    if (current.header.major != FIRMWARE_MAJOR_VERSION ||
        current.header.minor != FIRMWARE_MINOR_VERSION) {
        Serial.println("loadConfiguration: no configuration");
        // save default settings
        saveConfiguration(cfg);
    }
    else
        cfg = current;
}

void saveConfiguration(SwitchSettings &settings) {
    SwitchSettings current;

    EEPROM_readAnything(0, current);
    byte *s = (byte *)&settings;
    byte *c = (byte *)&current;
    for (unsigned int i = 0; i < sizeof(SwitchSettings); ++i) {
        if (c[i] == s[i])
            continue;
        Serial.print(i, HEX);
        Serial.print(" - c: ");
        Serial.print(c[i], HEX);
        Serial.print("s: ");
        Serial.println(s[i], HEX);
        EEPROM.write(i, s[i]);
    }
}

void handleReply() {
    Serial.println("handleReply: ");
    unsigned char *data = (unsigned char *)radio.Data;
    unsigned char offset = 0;
    bool settingsChanged = false;
    while (offset < *radio.DataLen) {
        SwitchPacket *header = (SwitchPacket *)(data + offset);
        switch (header->type) {
            case SwitchPacket::PING:
                Serial.println("ping");
                break;
            case SwitchPacket::CONFIGURE: {
                SwitchConfigure *pkt = (SwitchConfigure *)header;
                SwitchConfigureByte *b = (SwitchConfigureByte *)pkt->cfg;
                while (b < (SwitchConfigureByte *)((byte *)pkt + pkt->len)) {
                    Serial.print("set ");
                    Serial.print(b->offset, HEX);
                    Serial.print(" : ");
                    Serial.println(b->value, HEX);
                    byte *settings = (byte *)&cfg;
                    settings[b->offset] = b->value;
                    b++;
                }
                settingsChanged = true;
                break;
            }
            case SwitchPacket::DUMP_REQUEST: {
                SwitchDumpSettings pkt;
                Serial.println("dumping settings from EEPROM...");
                EEPROM_readAnything(0, pkt.settings);
                for (byte b = 0; b < sizeof(pkt.settings); ++b) {
                    Serial.print(*((byte *)&pkt.settings + b), HEX);
                    Serial.print(":");
                }
                Serial.println();
                radio.Send(GATEWAYID, (const void*)(&pkt), sizeof(pkt), false);
                break;
            }
            case SwitchPacket::RESET: {
                Serial.println("reset");
                SwitchReset *pkt = (SwitchReset *)header;
                if (pkt->resetSettings) {
                    Serial.println("resetting settings");
                    byte v = 255;
                    EEPROM_writeAnything(0, v);
                }
                softReset();
                break;
            }
            default:
                Serial.print("Unknown type: ");
                Serial.println(header->type);
                break;
        }
        offset += header->len;
    }

    if (settingsChanged) {
        saveConfiguration(cfg);
        softReset();
    }
}

void waitForReply(bool sleep = true) {
    long now = millis();
    while (millis() - now <= cfg.sleep.replyWakeLock) {
        if (radio.ACKReceived(GATEWAYID)) {
            handleReply();
            break;
        }
    }
    if (sleep)
        radio.Sleep(cfg.sleep.statusInterval, cfg.sleep.statusScaler);
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
    radio.Sleep(cfg.sleep.statusInterval, cfg.sleep.statusScaler);
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
    radio.Initialize(cfg.rfm12b.nodeId, DEFAULT_FREQ_BAND, NETWORKID);
    /* radio.Encrypt(KEY); */
    radio.Sleep(cfg.sleep.statusInterval, cfg.sleep.statusScaler);

    Serial.println("  * touch sensor...");
    touch.begin(cfg.mpr121);

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
            sleepPeriod = (period_t)cfg.sleep.touch;
        else if (touch.isProximity())
            sleepPeriod = (period_t)cfg.sleep.proximity;
        else
            sleepPeriod = (period_t)cfg.sleep.release;
        touch.wakeUp();
        touch.enableInterrupt();
    }
    else if (touch.isTouched() || touch.isProximity()) {
        // we woke up w/o an interrupt, but an electrode or proximity sensor
        // is still being touched - this is a repeat event
        touch.update();
        Serial.println("repeat:");
        handleEvent(1);
        sleepPeriod = (period_t)cfg.sleep.repeat;
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
