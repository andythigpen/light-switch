#ifndef SWITCHPROTOCOL_H
#define SWITCHPROTOCOL_H

#include "SwitchSettings.h"

struct SwitchPacket {
    SwitchPacket(unsigned char type, unsigned char len) :
        type(type), len(len) {}
    enum PacketType {
        TOUCH_EVENT,
        STATUS_UPDATE,
        RESET,
        CONFIGURE_SLEEP,
        CONFIGURE_MPR121,
        CONFIGURE_RFM12B,
        PING,
    };
    unsigned char type;
    unsigned char len;
};

struct TouchEvent : SwitchPacket {
    TouchEvent() : SwitchPacket(TOUCH_EVENT, sizeof(TouchEvent)) {}
    unsigned char gesture;
    unsigned char electrode;
    unsigned char repeat;
};

struct SwitchStatus : SwitchPacket {
    SwitchStatus() : SwitchPacket(STATUS_UPDATE, sizeof(SwitchStatus)) {}
    long batteryLevel;
};

struct SwitchReset : SwitchPacket {
    SwitchReset() : SwitchPacket(RESET, sizeof(SwitchReset)) {}
    unsigned char resetSettings;
};

struct SwitchConfigureSleep : SwitchPacket {
    SwitchConfigureSleep() :
        SwitchPacket(CONFIGURE_SLEEP, sizeof(SwitchConfigureSleep)) {}
    SleepSettings settings;
};

struct SwitchConfigureMPR121 : SwitchPacket {
    SwitchConfigureMPR121() :
        SwitchPacket(CONFIGURE_MPR121, sizeof(SwitchConfigureMPR121)) {}
    MPR121Settings settings;
};

struct SwitchConfigureRFM12B : SwitchPacket {
    SwitchConfigureRFM12B() :
        SwitchPacket(CONFIGURE_RFM12B, sizeof(SwitchConfigureRFM12B)) {}
    RFM12BSettings settings;
};

#endif // SWITCHPROTOCOL_H
