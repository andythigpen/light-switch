#ifndef TOUCHSEQUENCE_H
#define TOUCHSEQUENCE_H

#if ARDUINO >= 100
  #include <Arduino.h> // Arduino 1.0
#else
  #include <WProgram.h> // Arduino 0022
#endif

#include "TouchProtocol.h"

#ifndef MAX_TOUCH_SEQ
#define MAX_TOUCH_SEQ 4
#endif

struct Electrodes {
    byte total;
    byte top;
    byte bottom;
    byte left;
    byte right;
    byte center;
};

class TouchSequence {
    public:
        // Constructor
        //   mpr121Addr:   i2c address of the MPR121 IC
        //   interruptPin: external hardware interrupt.
        //                 NOTE: 0 for pin 2, 1 for pin 3
        TouchSequence(byte mpr121Addr, byte interruptPin);

        // Initialize the TouchSequence instance, MPR121 library
        //   electrodes:   number of electrodes to enable (1-12, 0 = disabled)
        //   proxMode:     0 = disabled
        //                 1 = electrodes 0-1
        //                 2 = electrodes 0-3
        //                 3 = electrodes 0-11
        void begin(byte electrodes, byte proxMode);
        // attaches the hardware interrupt
        // the interrupt will automatically be detached when it runs, so
        // enableInterrupt must be called again prior to going to sleep
        void enableInterrupt();
        // configure the electrode directions/total
        // NOTE: setting the total after calling begin() has no effect on
        //       the MPR121 IC.
        void setElectrodes(struct Electrodes &electrodes);

        // checks the sensors for any new inputs and adds them to the touch
        // sequence
        // returns true if an electrode is in touched state (including prox)
        // returns false if no electrodes are touched
        bool update();
        // clears the current sequence
        // call this after you've handled a touch sequence and you want to
        // start fresh
        void clear();

        // returns true if MPR121 external interrupt is active
        // returns false otherwise
        bool isInterrupted();

        // returns true if there is an ongoing touch event
        // returns false otherwise
        // modified by a call to update()
        bool isTouched();
        // returns true if there is an ongoing proximity event
        // returns false otherwise
        // modified by a call to update()
        bool isProximity();

        // returns electrode number of last touched event
        byte getLastTouch();

        // returns a gesture type
        // use getLastTouch() for TOUCH_TAP/TOUCH_DOUBLE_TAP to determine
        // which electrode was touched
        TouchGesture getGesture();

    protected:
        bool checkRepeatMode();
        TouchGesture checkShortSwipe();
        TouchGesture checkLongSwipe();
        TouchGesture checkTap();

        byte mpr121Addr;
        byte interruptPin;
        byte seq[MAX_TOUCH_SEQ];
        byte idx;
        bool touched;
        bool proximity;
        // true on a proximity event until a touch/clear
        bool proximityEvent;
        struct Electrodes electrodes;
};

#endif // TOUCHSEQUENCE_H
