#ifndef SWITCHPROTOCOL_H
#define SWITCHPROTOCOL_H

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

enum Electrode {
    ELECTRODE_TOP,
    ELECTRODE_LEFT,
    ELECTRODE_BOTTOM,
    ELECTRODE_RIGHT,
    ELECTRODE_CENTER,
    ELECTRODE_PROXIMITY = 12,
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
