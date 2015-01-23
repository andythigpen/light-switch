//
// Capacitive touch light switch hub
//
#include <RFM12B.h>
#include "SwitchProtocol.h"
#include "SwitchSettings.h"

RFM12B radio;

#define NODEID      1
#define NETWORKID   1
#define FREQUENCY   RF12_915MHZ
#define ACK_TIME    30  // # of ms to wait for an ack

void setup() {
    Serial.begin(115200);
    while (!Serial);

    radio.Initialize(NODEID, FREQUENCY, NETWORKID);
}

void handleTouchEvent() {
    if (*radio.DataLen != sizeof(TouchEvent)) {
        Serial.println("bad payload");
        return;
    }
    TouchEvent pkt = *(TouchEvent *)radio.Data;
    Serial.print(" event: ");
    Serial.print(pkt.gesture);
    Serial.print(" electrode:  ");
    Serial.print(pkt.electrode);
    Serial.print(" repeat:  ");
    Serial.print(pkt.repeat);
    Serial.println();
    switch (pkt.gesture) {
        case TOUCH_SWIPE_DOWN:
            Serial.println("swipe down");
            break;
        case TOUCH_SWIPE_UP:
            Serial.println("swipe up");
            break;
        case TOUCH_SWIPE_LEFT:
            Serial.println("swipe left");
            break;
        case TOUCH_SWIPE_RIGHT:
            Serial.println("swipe right");
            break;
        case TOUCH_TAP:
            Serial.print("tap ");
            Serial.println(pkt.electrode);
            break;
        case TOUCH_DOUBLE_TAP:
            Serial.print("double tap ");
            Serial.println(pkt.electrode);
            break;
        case TOUCH_PROXIMITY:
            Serial.println("proximity");
            break;
        default:
            Serial.println("no gesture");
            break;
    }
}

void handleStatusUpdate() {
    if (*radio.DataLen != sizeof(SwitchStatus)) {
        Serial.println("bad payload");
        return;
    }
    SwitchStatus pkt = *(SwitchStatus *)radio.Data;
    Serial.print(" vcc: ");
    Serial.print(pkt.batteryLevel);
    //TODO: send reconfiguration packet, if available
}

void loop() {
    if (radio.ReceiveComplete()) {
        if (radio.CRCPass()) {
            byte nodeid = radio.GetSender();
            Serial.print('[');
            Serial.print(nodeid);
            Serial.print(']');

            unsigned char type = *(unsigned char *)radio.Data;
            switch (type) {
                case SwitchPacket::TOUCH_EVENT:
                    handleTouchEvent();
                    break;
                case SwitchPacket::STATUS_UPDATE:
                    handleStatusUpdate();
                    break;
                default:
                    Serial.println("unknown event");
                    break;
            }

            if (radio.ACKRequested()) {
                SwitchPacket ping(SwitchPacket::PING, sizeof(SwitchPacket));
                Serial.println("ack:");
                radio.SendACK((void *)&ping, sizeof(ping));
                // radio.SendACK();
            }
            Serial.println();
        }
    }
}
