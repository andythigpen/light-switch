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
        CONFIGURE,
        PING,
        DUMP_REQUEST,
        DUMP_REPLY,
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

struct SwitchConfigureByte {
    unsigned char offset;
    unsigned char value;
};

struct SwitchConfigure : SwitchPacket {
    SwitchConfigure() : SwitchPacket(CONFIGURE, sizeof(SwitchConfigure)) {}
    unsigned char cfg[0];
};

struct SwitchDumpSettings : SwitchPacket {
    SwitchDumpSettings() : SwitchPacket(DUMP_REPLY, sizeof(SwitchDumpSettings)) {}
    SwitchSettings settings;
};

#endif // SWITCHPROTOCOL_H
