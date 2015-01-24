#ifndef TOUCHPROTOCOL_H
#define TOUCHPROTOCOL_H

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

enum PacketType {
    PKT_TOUCH_EVENT,
    PKT_STATUS_UPDATE,
};

struct TouchEvent {
    TouchEvent() : type(PKT_TOUCH_EVENT) {}
    unsigned char type;
    unsigned char gesture;
    unsigned char electrode;
    unsigned char repeat;
};

#endif // TOUCHPROTOCOL_H
