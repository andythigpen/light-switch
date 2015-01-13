//
// Capacitive touch light switch hub
//
#include <RFM12B.h>

RFM12B radio;

#define NODEID      1
#define NETWORKID   1
#define FREQUENCY   RF12_915MHZ
#define ACK_TIME    30  // # of ms to wait for an ack

typedef struct {
    int nodeid;
    int event;
    int electrode;
} Payload;
Payload data;

void setup() {
    Serial.begin(57600);
    while (!Serial);

    radio.Initialize(NODEID, FREQUENCY, NETWORKID);
}

void loop() {
    if (radio.ReceiveComplete()) {
        if (radio.CRCPass()) {
            Serial.print('[');
            Serial.print(radio.GetSender());
            Serial.print(']');

            if (*radio.DataLen != sizeof(Payload)) {
                Serial.println("bad payload");
            }
            else {
                data = *(Payload *)radio.Data;
                Serial.print("node:  ");
                Serial.print(data.nodeid);
                Serial.print(" event: ");
                Serial.print(data.event);
                Serial.print(" elec:  ");
                Serial.print(data.electrode);
            }

            if (radio.ACKRequested()) {
                byte nodeid = radio.GetSender();
                radio.SendACK();
            }
            Serial.println();
        }
    }
}
