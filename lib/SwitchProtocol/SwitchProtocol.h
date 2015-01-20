#ifndef SWITCHPROTOCOL_H
#define SWITCHPROTOCOL_H

#include "MPR121_conf.h"

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

struct SwitchSettings {
    struct {
        byte interval;
        byte scaler;
    } wakeUp;
    struct {
        byte touch;
        byte release;
        byte proximity;
        byte repeat;
    } sleepPeriod;
    struct {
        byte nodeId;
        byte networkId;
        byte gatewayId;  // probably shouldn't change this
        byte freqBand;
        byte txPower;
        byte airKbps;
        byte lowVoltageThreshold;
        //byte encryptionKey;  //TODO: changing this could sever communication
    } radio;
    struct MPR121Settings mpr121;
};

struct SwitchPacket {
    SwitchPacket(unsigned char type) : type(type) {}
    enum PacketType {
        TOUCH_EVENT,
        STATUS_UPDATE,
        RESET,
        CONFIGURE,
    };
    unsigned char type;
};

struct TouchEvent : SwitchPacket {
    TouchEvent() : SwitchPacket(TOUCH_EVENT) {}
    unsigned char gesture;
    unsigned char electrode;
    unsigned char repeat;
};

//TODO: add a version number to this packet?
//      no need to send it every time...maybe add another packet type that gets
//      sent on startup with version, and request for settings?
struct SwitchStatus : SwitchPacket {
    SwitchStatus() : SwitchPacket(STATUS_UPDATE) {}
    long batteryLevel;
};

struct SwitchReset : SwitchPacket {
    SwitchReset() : SwitchPacket(RESET) {}
    unsigned char hardReset;
};

struct SwitchConfigure : SwitchPacket {
    SwitchConfigure() : SwitchPacket(CONFIGURE) {}
    //TODO: use MPR121Settings here instead
    unsigned char touchThreshold;
    unsigned char releaseThreshold;
};

#endif // SWITCHPROTOCOL_H
