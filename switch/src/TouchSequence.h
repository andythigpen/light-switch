#ifndef TOUCHSEQUENCE_H
#define TOUCHSEQUENCE_H

#if ARDUINO >= 100
  #include <Arduino.h> // Arduino 1.0
#else
  #include <WProgram.h> // Arduino 0022
#endif

#include "SwitchProtocol.h"

#ifndef MAX_TOUCH_SEQ
#define MAX_TOUCH_SEQ 4
#endif

enum ElectrodeType {
    ELECTRODE_TOP,
    ELECTRODE_LEFT,
    ELECTRODE_BOTTOM,
    ELECTRODE_RIGHT,
    ELECTRODE_CENTER,
    ELECTRODE_PROXIMITY = 12,
};

struct Electrodes {
    byte top;
    byte bottom;
    byte left;
    byte right;
    byte center;
};

enum SleepMode {
    LOW_POWER_MODE,
    HIGH_SAMPLING_MODE,
};

class TouchSequence {
    public:
        // Constructor
        //   mpr121Addr:   i2c address of the MPR121 IC
        //   interruptPin: external hardware interrupt.
        //                 NOTE: 0 for pin 2, 1 for pin 3
        TouchSequence(byte mpr121Addr, byte interruptPin);

        // Initialize the TouchSequence instance, MPR121
        //   electrodes:    number of electrodes to enable (1-12, 0 = disabled)
        //   proximityMode: 0 = disabled
        //                  1 = electrodes 0-1
        //                  2 = electrodes 0-3
        //                  3 = electrodes 0-11
        void begin(byte electrodes, byte proximityMode);
        // attaches the hardware interrupt
        // the interrupt will automatically be detached when it runs, so
        // enableInterrupt must be called again prior to going to sleep
        void enableInterrupt();
        // configure the electrode directions/total
        //  total:      the number of electrodes to enable
        //  electrodes: directional electrodes used for gestures
        void setElectrodes(byte total, Electrodes &electrodes);

        // conserves power by reducing the sampling rate, filter rate of
        // the MPR121 to a lower level.
        // once an event is detected wakeUp should be called if the sampling
        // rate is too low to detect multiple events
        void sleep();
        // resets the MPR121 sampling/filter rates to the defaults
        void wakeUp();

        // checks the sensors for any new inputs and adds them to the touch
        // sequence
        // returns true if an electrode is in touched state (including prox)
        // returns false if no electrodes are touched
        bool update();
        // clears the current sequence
        // call this after you've handled a touch sequence and you want to
        // start over fresh
        void clear();

        // returns true if MPR121 external interrupt is active
        // returns false otherwise
        bool isInterrupted();

        // returns true if there is an ongoing touch event
        // returns false otherwise
        // modified by a call to update()
        bool isTouched();
        bool isTouched(byte electrode);

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

        // returns true if any of the electrodes/proximity sensors are on
        bool isRunning();
        // the default mode after calling begin()
        // in run mode, the MPR121 is actively taking measurements
        void run();
        // in stop mode, the electrodes are off and the MPR121 can be configured
        void stop();

        // updates the touch/release thresholds for the MPR121 sensor
        // val : range 0 - 255
        // ele : # of the electrode to set, if > 13, then all electrodes are
        //       set to val
        void setTouchThreshold(byte val, byte ele=0xFF);
        void setReleaseThreshold(byte val, byte ele=0xFF);

        // for debugging, prints out the registers of the MPR121
        void dump();

    protected:
        bool setRegister(byte reg, byte val);
        byte getRegister(byte reg);

        void applySettings(struct MPR121Settings&);
        void applyFilter(byte baseReg, struct MPR121Filter&);

        TouchGesture checkShortSwipe();
        TouchGesture checkLongSwipe();
        TouchGesture checkTap();

        byte interruptPin;
        byte seq[MAX_TOUCH_SEQ];
        byte idx;
        // true on a proximity event until a touch/clear
        bool proximityEvent;
        struct Electrodes electrodes;

        bool running;
        SleepMode sleepMode;

        struct {
            byte address;
            union {
                byte ecr;
                struct {
                    byte ele_en:4;
                    byte eleprox_en:2;
                    byte cl:2;
                };
            };
            union {
                int all;
                byte status[2];
                struct {
                    byte ele0:1;
                    byte ele1:1;
                    byte ele2:1;
                    byte ele3:1;
                    byte ele4:1;
                    byte ele5:1;
                    byte ele6:1;
                    byte ele7:1;
                    byte ele8:1;
                    byte ele9:1;
                    byte ele10:1;
                    byte ele11:1;
                    byte eleprox:1;
                    byte undef:2;
                    byte ovcf:1;
                };
            } touched;
        } mpr121;
};

#endif // TOUCHSEQUENCE_H
