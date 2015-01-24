#ifndef SWITCH_SETTINGS_H
#define SWITCH_SETTINGS_H

#include "MPR121_conf.h"
#include "LowPower.h"
#include "RFM12B.h"

// current version of the firmware
#define FIRMWARE_VERSION 1

// settings mask
#define SETTINGS_MPR121 0x01
#define SETTINGS_RFM12B 0x02
#define SETTINGS_SLEEP  0x04

// RFM12B default settings
#define GATEWAYID           1
#define DEFAULT_NODEID      127
#define DEFAULT_FREQ_BAND   RF12_915MHZ


enum TouchGesture {
    TOUCH_UNKNOWN,
    TOUCH_TAP,
    TOUCH_DOUBLE_TAP,
    TOUCH_SWIPE_UP,
    TOUCH_SWIPE_DOWN,
    TOUCH_SWIPE_LEFT,
    TOUCH_SWIPE_RIGHT,
    TOUCH_PROXIMITY,
};

struct SleepSettings {
    byte touch;
    byte release;
    byte proximity;
    byte repeat;
    byte replyWakeLock;
    byte statusInterval;
    byte statusScaler;

    SleepSettings() :
        touch(SLEEP_500MS),
        release(SLEEP_250MS),
        proximity(SLEEP_1S),
        repeat(SLEEP_250MS),
        replyWakeLock(30),      // keep mcu awake at most 30ms for ACK replies
        statusInterval(110),    // 150 * 2^11 = 3,604,480 ms ~= 1 hr
        statusScaler(15)
    {
    }
};

struct RFM12BSettings {
    byte nodeId;
    byte freqBand;
    byte txPower;
    byte airKbps;
    byte lowVoltageThreshold;

    RFM12BSettings() :
        nodeId(DEFAULT_NODEID),
        txPower(0),
        airKbps(0x08),
        lowVoltageThreshold(RF12_2v75)
    {
    }
};

#endif // SWITCH_SETTINGS_H
