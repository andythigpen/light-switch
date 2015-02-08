//
// Capacitive touch light switch hub
//
#include <RFM12B.h>
#include "SwitchProtocol.h"
#include "SwitchSettings.h"
#include "CmdMessenger.h"

RFM12B radio;

#define NODEID      1
#define NETWORKID   1
#define FREQUENCY   RF12_915MHZ
#define ACK_TIME    30  // # of ms to wait for an ack

#define CMD_MSG             0
#define CMD_ACK             1
#define CMD_TOUCH_EVENT     2
#define CMD_STATUS_EVENT    3
#define CMD_DUMP_SETTINGS   4
#define CMD_RESET           5
#define CMD_SET_BYTE        6
#define CMD_GET_I2C         7
#define CMD_SET_I2C         8

CmdMessenger cmd(Serial);
struct {
    byte nodeId;
    byte pkt[RF12_MAXDATA];
    byte *current;

    void reset() {
        nodeId = 0;
        current = pkt;
    }

    bool available(byte sz) {
        return sizeof(pkt) - (current - pkt) >= sz;
    }

    byte *reserve(byte nodeId, byte size) {
        if (nodeId != this->nodeId)
            reset();
        if (!available(size))
            return NULL;
        this->nodeId = nodeId;
        byte *ret = current;
        current += size;
        return ret;
    }

    byte length() {
        return current - pkt;
    }
} command;

void onUnknownCommand() {
    cmd.sendCmd(CMD_MSG, "Unknown command");
}

void onResetCommand() {
    byte nodeId = (byte)cmd.readCharArg();
    SwitchReset *rst = (SwitchReset *)command.reserve(nodeId, sizeof(SwitchReset));
    if (!rst) {
        cmd.sendCmd(CMD_MSG, "too many commands");
        return;
    }
    rst->type = SwitchPacket::RESET;
    rst->len = sizeof(SwitchReset);
    rst->resetSettings = cmd.readBoolArg() ? 1 : 0;
    cmd.sendCmd(CMD_ACK, "ok");
}

void onDumpCommand() {
    byte nodeId = (byte)cmd.readCharArg();
    SwitchDumpSettings *pkt = (SwitchDumpSettings *)command.reserve(
            nodeId, sizeof(SwitchPacket));
    if (!pkt) {
        cmd.sendCmd(CMD_MSG, "too many commands");
        return;
    }
    pkt->type = SwitchPacket::DUMP_REQUEST;
    pkt->len = sizeof(SwitchPacket);
    cmd.sendCmd(CMD_ACK, "ok");
}

void onSetByteCommand() {
    byte nodeId = (byte)cmd.readInt16Arg();
    byte offset = (byte)cmd.readInt16Arg();
    byte value = (byte)cmd.readInt16Arg();
    SwitchConfigure *pkt = (SwitchConfigure *)command.reserve(
            nodeId, sizeof(SwitchConfigure));
    if (!pkt) {
        cmd.sendCmd(CMD_MSG, "too many commands");
        return;
    }
    pkt->type = SwitchPacket::CONFIGURE;
    pkt->len = sizeof(SwitchConfigure);
    pkt->cfg.offset = offset;
    pkt->cfg.value = value;
    cmd.sendCmdStart(CMD_ACK);
    cmd.sendCmdArg(nodeId);
    cmd.sendCmdArg(offset);
    cmd.sendCmdArg(value);
    cmd.sendCmdEnd();
}

void onGetI2C() {
    byte nodeId = (byte)cmd.readInt16Arg();
    byte address = (byte)cmd.readInt16Arg();
    byte reg = (byte)cmd.readInt16Arg();
    SwitchI2CRequest *pkt = (SwitchI2CRequest *)command.reserve(
            nodeId, sizeof(SwitchI2CRequest));
    if (!pkt) {
        cmd.sendCmd(CMD_MSG, "too many commands");
        return;
    }
    pkt->type = SwitchPacket::I2C_REQUEST;
    pkt->len = sizeof(SwitchI2CRequest);
    pkt->address = address;
    pkt->reg = reg;
}

void onSetI2C() {
    byte nodeId = (byte)cmd.readInt16Arg();
    byte address = (byte)cmd.readInt16Arg();
    byte reg = (byte)cmd.readInt16Arg();
    byte val = (byte)cmd.readInt16Arg();
    SwitchI2CSet *pkt = (SwitchI2CSet *)command.reserve(
            nodeId, sizeof(SwitchI2CSet));
    if (!pkt) {
        cmd.sendCmd(CMD_MSG, "too many commands");
        return;
    }
    pkt->type = SwitchPacket::I2C_SET;
    pkt->len = sizeof(SwitchI2CSet);
    pkt->address = address;
    pkt->reg = reg;
    pkt->val = val;
    cmd.sendCmdStart(CMD_ACK);
    cmd.sendCmdArg(nodeId);
    cmd.sendCmdArg(address);
    cmd.sendCmdArg(reg);
    cmd.sendCmdArg(val);
    cmd.sendCmdEnd();
}

void setup() {
    Serial.begin(115200);
    radio.Initialize(NODEID, FREQUENCY, NETWORKID);

    command.reset();

    cmd.attach(onUnknownCommand);
    cmd.attach(CMD_RESET, onResetCommand);
    cmd.attach(CMD_DUMP_SETTINGS, onDumpCommand);
    cmd.attach(CMD_SET_BYTE, onSetByteCommand);
    cmd.attach(CMD_GET_I2C, onGetI2C);
    cmd.attach(CMD_SET_I2C, onSetI2C);
    cmd.sendCmd(CMD_MSG, "Initialized...");
}

void handleTouchEvent(byte nodeId) {
    if (*radio.DataLen != sizeof(TouchEvent)) {
        cmd.sendCmd(CMD_MSG, "bad touch event payload");
        return;
    }
    TouchEvent pkt = *(TouchEvent *)radio.Data;
    cmd.sendCmdStart(CMD_TOUCH_EVENT);
    cmd.sendCmdArg(nodeId);
    cmd.sendCmdArg(pkt.gesture);
    cmd.sendCmdArg(pkt.electrode);
    cmd.sendCmdArg(pkt.repeat);
    cmd.sendCmdEnd();
}

void handleStatusUpdate(byte nodeId) {
    if (*radio.DataLen != sizeof(SwitchStatus)) {
        cmd.sendCmd(CMD_MSG, "bad status event payload");
        return;
    }
    SwitchStatus pkt = *(SwitchStatus *)radio.Data;
    cmd.sendCmdStart(CMD_STATUS_EVENT);
    cmd.sendCmdArg(nodeId);
    cmd.sendCmdArg(pkt.batteryLevel);
    cmd.sendCmdArg(pkt.statusCount);
    cmd.sendCmdEnd();
}

void handleSettingsDump(byte nodeId) {
    if (*radio.DataLen != sizeof(SwitchDumpSettings)) {
        cmd.sendCmd(CMD_MSG, "bad settings dump payload");
        return;
    }
    SwitchDumpSettings pkt = *(SwitchDumpSettings *)radio.Data;
    cmd.sendCmdStart(CMD_DUMP_SETTINGS);
    cmd.sendCmdArg(nodeId);
    cmd.sendCmdBinArg(pkt.settings);
    cmd.sendCmdEnd();
}

void handleI2CReply(byte nodeId) {
    if (*radio.DataLen != sizeof(SwitchI2CReply)) {
        cmd.sendCmd(CMD_MSG, "bad i2c reply payload");
        return;
    }
    SwitchI2CReply pkt = *(SwitchI2CReply *)radio.Data;
    cmd.sendCmdStart(CMD_GET_I2C);
    cmd.sendCmdArg(nodeId);
    cmd.sendCmdArg(pkt.address);
    cmd.sendCmdArg(pkt.reg);
    cmd.sendCmdArg(pkt.val);
    cmd.sendCmdEnd();
}

void handleIncomingPacket() {
    if (!radio.CRCPass()) {
        return;
    }

    byte nodeId = radio.GetSender();
    byte data[RF12_MAXDATA];
    byte datalen = *radio.DataLen;
    memcpy(data, (void *)radio.Data, datalen);
    unsigned char offset = 0;
    while (offset < datalen) {
        SwitchPacket *header = (SwitchPacket *)(data + offset);
        switch (header->type) {
            case SwitchPacket::TOUCH_EVENT:
                handleTouchEvent(nodeId);
                break;
            case SwitchPacket::STATUS_UPDATE:
                handleStatusUpdate(nodeId);
                break;
            case SwitchPacket::DUMP_REPLY:
                handleSettingsDump(nodeId);
                break;
            case SwitchPacket::I2C_REPLY:
                handleI2CReply(nodeId);
                break;
            default:
                cmd.sendCmd(CMD_MSG, "unknown event");
                break;
        }
        offset += header->len;
    }

    if (radio.ACKRequested()) {
        if (command.nodeId == nodeId) {
            cmd.sendCmd(0, "Sending command packet");
            radio.SendACK((void *)&command.pkt, command.length());
            command.reset();
        }
        else
            radio.SendACK();
    }
}

void loop() {
    if (radio.ReceiveComplete()) {
        handleIncomingPacket();
    }
    cmd.feedinSerialData();
}
