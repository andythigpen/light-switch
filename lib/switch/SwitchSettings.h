#ifndef SWITCH_SETTINGS_H
#define SWITCH_SETTINGS_H

#include "MPR121_conf.h"
#include "LowPower.h"
#include "RFM12B.h"

// current version of the firmware
#define FIRMWARE_MAJOR_VERSION  0
#define FIRMWARE_MINOR_VERSION  1

// RFM12B default settings
#define GATEWAYID           1
#ifndef DEFAULT_NODEID
#define DEFAULT_NODEID      127
#endif
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
        statusInterval(110),    // interval * 2^scaler = status update time (ms)
        statusScaler(15)        // 110 * 2^15 = 3,604,480 ms ~= 1 hr
    {
    }
};

struct RFM12BSettings {
    byte nodeId;
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

struct SwitchSettings {
    struct Header {
        byte major;
        byte minor;

        Header() : major(FIRMWARE_MAJOR_VERSION), minor(FIRMWARE_MINOR_VERSION) {}
    } header;
    RFM12BSettings rfm12b;
    MPR121Settings mpr121;
    SleepSettings sleep;
};

#endif // SWITCH_SETTINGS_H
