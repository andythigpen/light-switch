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
    byte pkt[256];
} command;

void onUnknownCommand() {
    cmd.sendCmd(CMD_MSG, "Unknown command");
}

void onResetCommand() {
    command.nodeId = (byte)cmd.readCharArg();
    SwitchReset *rst = (SwitchReset *)&command.pkt;
    rst->type = SwitchPacket::RESET;
    rst->len = sizeof(SwitchReset);
    rst->resetSettings = cmd.readBoolArg() ? 1 : 0;
    cmd.sendCmd(CMD_ACK, "ok");
}

void onDumpCommand() {
    command.nodeId = (byte)cmd.readCharArg();
    SwitchDumpSettings *pkt = (SwitchDumpSettings *)&command.pkt;
    pkt->type = SwitchPacket::DUMP_REQUEST;
    pkt->len = sizeof(SwitchPacket);
    cmd.sendCmd(CMD_ACK, "ok");
}

void onSetByteCommand() {
    byte nodeId = (byte)cmd.readInt16Arg();
    byte offset = (byte)cmd.readInt16Arg();
    byte value = (byte)cmd.readInt16Arg();
    SwitchConfigure *pkt = (SwitchConfigure *)&command.pkt;
    if (nodeId == command.nodeId && pkt->type == SwitchPacket::CONFIGURE) {
        byte *p = (byte *)pkt;
        p[pkt->len++] = offset;
        p[pkt->len++] = value;
    }
    else {
        command.nodeId = nodeId;
        pkt->type = SwitchPacket::CONFIGURE;
        pkt->len = sizeof(SwitchConfigure) + sizeof(SwitchConfigureByte);
        pkt->cfg[0] = offset;
        pkt->cfg[1] = value;
    }
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
    SwitchI2CRequest *pkt = (SwitchI2CRequest *)&command.pkt;
    command.nodeId = nodeId;
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
    SwitchI2CSet *pkt = (SwitchI2CSet *)&command.pkt;
    command.nodeId = nodeId;
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
    cmd.sendCmdStart(CMD_ACK);
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
    unsigned char type = *(unsigned char *)radio.Data;
    switch (type) {
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

    if (radio.ACKRequested()) {
        if (command.nodeId == nodeId) {
            cmd.sendCmd(0, "Sending command packet");
            SwitchPacket *hdr = (SwitchPacket *)&command.pkt;
            radio.SendACK((void *)&command.pkt, hdr->len);
            command.nodeId = 0;
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
