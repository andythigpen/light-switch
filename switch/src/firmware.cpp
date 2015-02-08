//
// Capacitive touch light switch
//
#include <Arduino.h>
#include <avr/wdt.h>
#include <EEPROM.h>
#include <Wire.h>
#include "LowPower.h"
#include "TouchSequence.h"
#include "RFM12B.h"
#include "SwitchProtocol.h"
#include "SwitchSettings.h"
#include "util.h"
#include "debug.h"

#ifndef NETWORKID
#error "NETWORKID must be defined!"
#endif

static const int mpr121Addr         = 0x5A;
static const int mpr121IntPin       = 1;    // int 1 == pin 3

static SwitchSettings cfg;
static unsigned int statusCount     = 0;

RFM12B radio;
TouchSequence touch(mpr121Addr, mpr121IntPin);
period_t sleepPeriod = SLEEP_FOREVER;

extern long readVcc();
void sendStatus();

void softReset() {
#if !defined(NDEBUG)
    DEBUG("softReset:");
    Serial.flush();
#endif
    asm volatile ("  jmp 0");
}

void sleep(period_t time) {
#if !defined(NDEBUG)
    DEBUG("sleep: ", time);
    // flush serial for debugging before powering down
    Serial.flush();
    delay(100);
#endif

    // turn off the wdt in case it was previously set...this is to prevent a
    // spurious wakeup after an interrupt and then sleep forever call.
    if (time == SLEEP_FOREVER) {
        wdt_disable();
        LowPower.powerDown(time, ADC_OFF, BOD_OFF);
    }
    else
        LowPower.powerStandby(time, ADC_OFF, BOD_OFF);
}

void saveConfiguration(SwitchSettings &settings) {
    SwitchSettings current;

    EEPROM_readAnything(0, current);
    byte *s = (byte *)&settings;
    byte *c = (byte *)&current;
    for (unsigned int i = 0; i < sizeof(SwitchSettings); ++i) {
        if (c[i] == s[i])
            continue;
        DEBUG_("byte [");
        DEBUG_FMT_(i, HEX);
        DEBUG_("] current: ");
        DEBUG_FMT_(c[i], HEX);
        DEBUG_(" new: ");
        DEBUG_FMT(s[i], HEX);
        EEPROM.write(i, s[i]);
    }
}

void loadConfiguration() {
    DEBUG("loadConfiguration:");

    SwitchSettings current;
    EEPROM_readAnything(0, current);
    if (current.header.major != FIRMWARE_MAJOR_VERSION ||
        current.header.minor != FIRMWARE_MINOR_VERSION) {
        DEBUG("loadConfiguration: no configuration");
        // save default settings
        saveConfiguration(cfg);
    }
    else
        cfg = current;
}

void handleReply() {
    DEBUG("handleReply: ", *radio.DataLen);
    byte data[RF12_MAXDATA];
    byte datalen = *radio.DataLen;
    memcpy(data, (void *)radio.Data, datalen);
    unsigned char offset = 0;
    bool settingsChanged = false;
    while (offset < datalen) {
        SwitchPacket *header = (SwitchPacket *)(data + offset);
        DEBUG("header type: ", header->type);
        switch (header->type) {
            case SwitchPacket::PING:
                DEBUG("ping");
                break;
            case SwitchPacket::STATUS_REQUEST:
                DEBUG("status request: ");
                sendStatus();
                break;
            case SwitchPacket::CONFIGURE: {
                SwitchConfigure *pkt = (SwitchConfigure *)header;
                DEBUG_("set ");
                DEBUG_FMT_(pkt->cfg.offset, HEX);
                DEBUG_(" : ");
                DEBUG_FMT(pkt->cfg.value, HEX);
                byte *settings = (byte *)&cfg;
                settings[pkt->cfg.offset] = pkt->cfg.value;
                settingsChanged = true;
                break;
            }
            case SwitchPacket::DUMP_REQUEST: {
                SwitchDumpSettings pkt;
                DEBUG("dumping settings from EEPROM...");
                EEPROM_readAnything(0, pkt.settings);
                for (byte b = 0; b < sizeof(pkt.settings); ++b) {
                    DEBUG_FMT_(*((byte *)&pkt.settings + b), HEX);
                    DEBUG_(":");
                }
                DEBUG("");
                radio.Send(GATEWAYID, (const void*)(&pkt), sizeof(pkt), false);
                break;
            }
            case SwitchPacket::RESET: {
                DEBUG("reset");
                SwitchReset *pkt = (SwitchReset *)header;
                if (pkt->resetSettings) {
                    DEBUG("resetting settings");
                    byte v = 255;
                    EEPROM_writeAnything(0, v);
                }
                softReset();
                break;
            }
            case SwitchPacket::I2C_REQUEST: {
                SwitchI2CRequest *request = (SwitchI2CRequest *)header;
                SwitchI2CReply reply;

                Wire.beginTransmission(request->address);
                Wire.write(request->reg);
                Wire.endTransmission(false);
                Wire.requestFrom(request->address, (byte)1);
                Wire.endTransmission();

                reply.address = request->address;
                reply.reg = request->reg;
                reply.val = Wire.read();
                DEBUG("i2c request: ", reply.address, " ", reply.reg, " ",
                        reply.val);
                radio.Send(GATEWAYID, (const void*)(&reply), sizeof(reply), false);
                break;
            }
            case SwitchPacket::I2C_SET: {
                SwitchI2CSet *pkt = (SwitchI2CSet *)header;
                bool success = false;
                DEBUG("i2c set: ", pkt->address, " ", pkt->reg, " ", pkt->val);
                if (pkt->address == mpr121Addr) {
                    success = touch.setRegister(pkt->reg, pkt->val);
                }
                else {
                    Wire.beginTransmission(pkt->address);
                    Wire.write(pkt->reg);
                    Wire.write(pkt->val);
                    byte errorCode = Wire.endTransmission();
                    success = errorCode == 0;
                }
                DEBUG("i2c set: ", success ? "success" : "error");
                break;
            }
            default:
                DEBUG("Unknown type: ", header->type);
                break;
        }
        offset += header->len;
        DEBUG("offset: ", offset);
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
#if !defined(NDEBUG)
    switch (pkt.gesture) {
        case TOUCH_SWIPE_DOWN:  DEBUG("swipe down");   break;
        case TOUCH_SWIPE_UP:    DEBUG("swipe up");     break;
        case TOUCH_SWIPE_LEFT:  DEBUG("swipe left");   break;
        case TOUCH_SWIPE_RIGHT: DEBUG("swipe right");  break;
        case TOUCH_PROXIMITY:   DEBUG("proximity");    break;
        case TOUCH_TAP:
            DEBUG("tap ", pkt.electrode);
            break;
        case TOUCH_DOUBLE_TAP:
            DEBUG("double tap ", pkt.electrode);
            break;
        default:
            DEBUG("no gesture");
            return;
    }
#endif
    radio.Wakeup();
    radio.Send(GATEWAYID, (const void*)(&pkt), sizeof(pkt), !repeated);
    if (!repeated)
        waitForReply();
}

void sendStatus() {
    SwitchStatus pkt;
    pkt.batteryLevel = readVcc();
    pkt.statusCount = statusCount++;
    DEBUG("vcc: ", pkt.batteryLevel, " cnt: ", pkt.statusCount);

    radio.Wakeup();
    radio.Send(GATEWAYID, (const void*)(&pkt), sizeof(pkt), true);
    waitForReply();
}

void setup() {
    // initialize serial
    Serial.begin(115200);
    while (!Serial);
    DEBUG("initializing...");

    // D5 GND for electrodes
    //pinMode(5, OUTPUT);
    //digitalWrite(5, LOW);

    loadConfiguration();

    DEBUG("  * radio...");
    radio.Initialize(cfg.rfm12b.nodeId, DEFAULT_FREQ_BAND, NETWORKID);
    /* radio.Encrypt(KEY); */
    radio.Sleep(cfg.sleep.statusInterval, cfg.sleep.statusScaler);

    DEBUG("  * touch sensor...");
    touch.begin(cfg.mpr121);

    // proximity thresholds
    touch.stop();
    touch.setTouchThreshold(2, 12);
    touch.setReleaseThreshold(1, 12);
    touch.run();

    touch.enableInterrupt();
    touch.dump();

    sendStatus();
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
        DEBUG("event: ", millis());
        touch.enableInterrupt();
    }
    else if (touch.isTouched() || touch.isProximity()) {
        // we woke up w/o an interrupt, but an electrode or proximity sensor
        // is still being touched - this is a repeat event
        touch.update();
        DEBUG("repeat:");
        handleEvent(1);
        sleepPeriod = (period_t)cfg.sleep.repeat;
    }
    else {
        // no touch interrupt, no current touch - this is a timeout.
        // handle the event and then clear everything to reset.
        handleEvent(0);
        touch.clear();
        DEBUG("touch done:", millis());
        DEBUG("");
        sleepPeriod = SLEEP_FOREVER;
    }
}
