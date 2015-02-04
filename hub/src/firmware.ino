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
#define CMD_TOUCH_EVENT     1
#define CMD_STATUS_EVENT    2
#define CMD_DUMP_SETTINGS   3

CmdMessenger cmd(Serial);

void setup() {
    Serial.begin(115200);
    radio.Initialize(NODEID, FREQUENCY, NETWORKID);

    cmd.attach(onUnknownCommand);
    cmd.sendCmd(CMD_MSG, "Initialized...");
}

void onUnknownCommand() {
    cmd.sendCmd(CMD_MSG, "Unknown command");
}

void handleTouchEvent() {
    if (*radio.DataLen != sizeof(TouchEvent)) {
        cmd.sendCmd(CMD_MSG, "bad touch event payload");
        return;
    }
    TouchEvent pkt = *(TouchEvent *)radio.Data;
    cmd.sendCmdStart(CMD_TOUCH_EVENT);
    cmd.sendCmdArg(radio.GetSender());
    cmd.sendCmdArg(pkt.gesture);
    cmd.sendCmdArg(pkt.electrode);
    cmd.sendCmdArg(pkt.repeat);
    cmd.sendCmdEnd();
}

void handleStatusUpdate() {
    if (*radio.DataLen != sizeof(SwitchStatus)) {
        cmd.sendCmd(CMD_MSG, "bad status event payload");
        return;
    }
    SwitchStatus pkt = *(SwitchStatus *)radio.Data;
    cmd.sendCmdStart(CMD_STATUS_EVENT);
    cmd.sendCmdArg(radio.GetSender());
    cmd.sendCmdArg(pkt.batteryLevel);
    cmd.sendCmdEnd();
}

void handleSettingsDump() {
    if (*radio.DataLen != sizeof(SwitchDumpSettings)) {
        cmd.sendCmd(CMD_MSG, "bad settings dump payload");
        return;
    }
    SwitchDumpSettings pkt = *(SwitchDumpSettings *)radio.Data;
    cmd.sendCmdStart(CMD_DUMP_SETTINGS);
    cmd.sendCmdArg(radio.GetSender());
    cmd.sendCmdBinArg(pkt.settings);
    cmd.sendCmdEnd();
}

void handleIncomingPacket() {
    if (!radio.CRCPass()) {
        return;
    }

    unsigned char type = *(unsigned char *)radio.Data;
    switch (type) {
        case SwitchPacket::TOUCH_EVENT:
            handleTouchEvent();
            break;
        case SwitchPacket::STATUS_UPDATE:
            handleStatusUpdate();
            break;
        case SwitchPacket::DUMP_REPLY:
            handleSettingsDump();
            break;
        default:
            cmd.sendCmd(CMD_MSG, "unknown event");
            break;
    }

    if (radio.ACKRequested()) {
        //TODO: send reconfiguration packet, if available
        SwitchPacket ping(SwitchPacket::PING, sizeof(SwitchPacket));
        radio.SendACK((void *)&ping, sizeof(ping));
    }
}

void loop() {
    if (radio.ReceiveComplete()) {
        handleIncomingPacket();
    }
    cmd.feedinSerialData();
}
