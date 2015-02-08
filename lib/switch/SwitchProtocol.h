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
        I2C_REQUEST,
        I2C_REPLY,
        I2C_SET,
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
    unsigned int statusCount;
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
    SwitchConfigureByte cfg;
};

struct SwitchDumpSettings : SwitchPacket {
    SwitchDumpSettings() : SwitchPacket(DUMP_REPLY, sizeof(SwitchDumpSettings)) {}
    SwitchSettings settings;
};

struct SwitchI2CRequest : SwitchPacket {
    SwitchI2CRequest() : SwitchPacket(I2C_REQUEST, sizeof(SwitchI2CRequest)) {}
    unsigned char address;
    unsigned char reg;
};

struct SwitchI2CReply : SwitchPacket {
    SwitchI2CReply() : SwitchPacket(I2C_REPLY, sizeof(SwitchI2CReply)) {}
    unsigned char address;
    unsigned char reg;
    unsigned char val;
};

struct SwitchI2CSet : SwitchPacket {
    SwitchI2CSet() : SwitchPacket(I2C_SET, sizeof(SwitchI2CSet)) {}
    unsigned char address;
    unsigned char reg;
    unsigned char val;
};

#endif // SWITCHPROTOCOL_H
