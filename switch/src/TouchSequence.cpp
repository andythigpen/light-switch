#include "TouchSequence.h"
//#include "MPR121.h"
#include <Wire.h>
#include "MPR121_registers.h"


#define DEBUG_TOUCH
//TODO: add a debug macro for printing to serial port if DEBUG_TOUCH is defined

struct MPR121Filter {
    byte mhd;
    byte nhd;
    byte ncl;
    byte fdl;
};

struct MPR121FilterGroup {
    MPR121Filter rising;
    MPR121Filter falling;
    MPR121Filter touched;
};

struct MPR121Settings {
    // filter configuration
    struct MPR121FilterGroup electrode;
    struct MPR121FilterGroup proximity;

    // debounce settings
    byte debounce;

    // configuration registers
    byte afe1;
    byte afe2;

    // auto-configuration registers
    byte accr0;
    byte accr1;
    byte usl;
    byte lsl;
    byte tl;

    MPR121Settings() :
        debounce(0x11),
        afe1(0xFF),
        afe2(0x38),
        accr0(0x00),
        accr1(0x00),
        usl(200),
        lsl(100),
        tl(180)
    {
        electrode.rising  = (MPR121Filter){ 0x3F, 0x3F, 0x05, 0x00 };
        electrode.falling = (MPR121Filter){ 0x01, 0x3F, 0x10, 0x03 };
        electrode.touched = (MPR121Filter){ 0x00, 0x01, 0x01, 0xFF };
        proximity.rising  = (MPR121Filter){ 0x0F, 0x0F, 0x00, 0x00 };
        proximity.falling = (MPR121Filter){ 0x01, 0x01, 0xFF, 0xFF };
        proximity.touched = (MPR121Filter){ 0x00, 0x00, 0x00, 0x00 };
    }
};

struct MPR121ConfigLock {
    MPR121ConfigLock(TouchSequence *inst, bool acquireImmediately=true) :
        shouldLock(false), inst(inst)
    {
        if (acquireImmediately)
            acquire();
    }
    ~MPR121ConfigLock() {
        release();
    }
    void acquire() {
        shouldLock = inst->isRunning();
        if (shouldLock)
            inst->sleep(SLEEP_ALL_OFF);
    }
    void release() {
        if (shouldLock)
            inst->wakeUp();
        shouldLock = false;
    }
    bool shouldLock;
    TouchSequence *inst;
};

static struct MPR121Settings defaultSettings;

static void wakeUpInt0() {
    detachInterrupt(0);
}

static void wakeUpInt1() {
    detachInterrupt(1);
}

TouchSequence::TouchSequence(byte mpr121Addr, byte interruptPin) :
    mpr121Addr(mpr121Addr), interruptPin(interruptPin), idx(0),
    proximityEvent(false), running(false)
{
    electrodes.top    = ELECTRODE_TOP;
    electrodes.left   = ELECTRODE_LEFT;
    electrodes.bottom = ELECTRODE_BOTTOM;
    electrodes.right  = ELECTRODE_RIGHT;
    electrodes.center = ELECTRODE_CENTER;
}

void
TouchSequence::begin(byte electrodes, byte proximityMode)
{
    Wire.begin();

    mpr121.ele_en = electrodes & 0x0F;
    mpr121.eleprox_en = proximityMode & 0x03;
    mpr121.cl = 2;

    MPR121Settings defaults;
    applySettings(defaults);
    // default settings are applied, it's time to run...
    wakeUp();
    clear();
}

bool
TouchSequence::setRegister(byte reg, byte val)
{
    MPR121ConfigLock lock(this, reg != ECR && reg < 0x72);

    Wire.beginTransmission(mpr121Addr);
    Wire.write(reg);
    Wire.write(val);
    errorCode = Wire.endTransmission();

    // automatically update the running flag if ECR is set
    if (errorCode == 0 && reg == ECR)
        running = val & 0x3F;

    return errorCode == 0;
}

byte
TouchSequence::getRegister(byte reg)
{
    Wire.beginTransmission(mpr121Addr);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(mpr121Addr, (byte)1);
    errorCode = Wire.endTransmission();
    return Wire.read();
}

void
TouchSequence::applyFilter(byte baseReg, struct MPR121Filter &filter)
{
    // touch state does not have MHD register
    if (baseReg != NHDT && baseReg != NHDPROXT)
        setRegister(baseReg++, filter.mhd);
    setRegister(baseReg++, filter.nhd);
    setRegister(baseReg++, filter.ncl);
    setRegister(baseReg++, filter.fdl);
}

void
TouchSequence::setTouchThreshold(byte val, byte reg)
{
    MPR121ConfigLock lock(this);
    if (reg < 13)
        setRegister(E0TTH + (reg << 1), val);
    else {
        for (int i = 0; i < 13; ++i)
            setRegister(E0TTH + (i << 1), val);
    }
}

void
TouchSequence::setReleaseThreshold(byte val, byte reg)
{
    MPR121ConfigLock lock(this);
    if (reg < 13)
        setRegister(E0RTH + (reg << 1), val);
    else {
        for (int i = 0; i < 13; ++i)
            setRegister(E0RTH + (i << 1), val);
    }
}

void
TouchSequence::applySettings(MPR121Settings &settings)
{
    MPR121ConfigLock lock(this);

    applyFilter(MHDR, settings.electrode.rising);
    applyFilter(MHDF, settings.electrode.falling);
    applyFilter(NHDT, settings.electrode.touched);
    applyFilter(MHDPROXR, settings.proximity.rising);
    applyFilter(MHDPROXF, settings.proximity.falling);
    applyFilter(NHDPROXT, settings.proximity.touched);

    setRegister(DBR, settings.debounce);
    setRegister(AFE1, settings.afe1);
    setRegister(AFE2, settings.afe2);
    setRegister(ACCR0, settings.accr0);
    setRegister(ACCR1, settings.accr1);
    setRegister(ACUSL, settings.usl);
    setRegister(ACLSL, settings.lsl);
    setRegister(ACTL, settings.tl);
}

void
TouchSequence::setElectrodes(byte total, struct Electrodes &electrodes)
{
    mpr121.ele_en = total & 0x0F;
    setRegister(ECR, mpr121.ecr);

    this->electrodes.top    = electrodes.top;
    this->electrodes.left   = electrodes.left;
    this->electrodes.bottom = electrodes.bottom;
    this->electrodes.right  = electrodes.right;
    this->electrodes.center = electrodes.center;
}

void
TouchSequence::sleep(SleepMode mode)
{
    if (!running)
        return;
    byte mask;
    switch (mode) {
        case SLEEP_ELECTRODES_OFF:  mask = 0xF0; break;
        case SLEEP_PROXIMITY_OFF:   mask = 0xCF; break;
        case SLEEP_ALL_OFF:         mask = 0xC0; break;
    }
    // mask the lower bits to turn proximity/touch electrodes off
    setRegister(ECR, mpr121.ecr & mask);
}

// bool
// TouchSequence::wasAsleep()
// {
//     //TODO:
//     return false;
// }

bool
TouchSequence::isRunning()
{
    return running;
}

void
TouchSequence::wakeUp()
{
    if (running)
        return;
    // restore the current configuration
    setRegister(ECR, mpr121.ecr);
}

bool
TouchSequence::update()
{
#if defined(DEBUG_TOUCH)
    Serial.println("updating sensors:");
#endif
    int prevTouchedState = mpr121.touched.all;
    // clears the interrupt
    mpr121.touched.status[0] = getRegister(ELE0_7);
    mpr121.touched.status[1] = getRegister(ELE8_PROX);

    for (byte i = 0; i < mpr121.ele_en; ++i) {
        if (!isTouched(i))
            continue;
        // test to see if there was a change
        if (prevTouchedState ^ mpr121.touched.all) {
            if (mpr121.touched.all & (1 << i)) {
#if defined(DEBUG_TOUCH)
                Serial.print("touch: ");
#endif
                seq[idx++] = i;
                if (idx >= MAX_TOUCH_SEQ)
                    idx = MAX_TOUCH_SEQ;
            }
#if defined(DEBUG_TOUCH)
            else
                Serial.print("release: ");
#endif
        }
#if defined(DEBUG_TOUCH)
        else
            Serial.print("repeat: ");
        Serial.println(i, DEC);
#endif
    }

    if (mpr121.touched.eleprox && seq[0] == 0xFF)
        proximityEvent = true;
    else if (seq[0] != 0xFF)
        proximityEvent = false;

    return mpr121.touched.all & 0x1FF;
}

void
TouchSequence::clear()
{
    for (int i = 0; i < MAX_TOUCH_SEQ; ++i)
        seq[i] = 0xFF;
    idx = 0;
    mpr121.touched.all = 0;
    proximityEvent = false;
}

TouchGesture
TouchSequence::checkLongSwipe()
{
    if (idx <= 1)
        return TOUCH_UNKNOWN;

    if (seq[0] == electrodes.top &&
        seq[idx - 1] == electrodes.bottom)
        return TOUCH_SWIPE_DOWN;

    if (seq[0] == electrodes.bottom &&
        seq[idx - 1] == electrodes.top)
        return TOUCH_SWIPE_UP;

    if (seq[0] == electrodes.left &&
        seq[idx - 1] == electrodes.right)
        return TOUCH_SWIPE_RIGHT;

    if (seq[0] == electrodes.right &&
        seq[idx - 1] == electrodes.left)
        return TOUCH_SWIPE_LEFT;

    return TOUCH_UNKNOWN;
}

TouchGesture
TouchSequence::checkShortSwipe()
{
    // short swipes start from the center towards one other direction
    if (idx != 2)
        return TOUCH_UNKNOWN;

    if (seq[0] == electrodes.center &&
        seq[1] == electrodes.bottom)
        return TOUCH_SWIPE_DOWN;

    if (seq[0] == electrodes.center &&
        seq[1] == electrodes.top)
        return TOUCH_SWIPE_UP;

    if (seq[0] == electrodes.center &&
        seq[1] == electrodes.right)
        return TOUCH_SWIPE_RIGHT;

    if (seq[0] == electrodes.center &&
        seq[1] == electrodes.left)
        return TOUCH_SWIPE_LEFT;

    return TOUCH_UNKNOWN;
}

TouchGesture
TouchSequence::checkTap()
{
    // must have at least one valid touch entry
    if (seq[0] == 0xFF)
        return TOUCH_UNKNOWN;

    if (idx == 1)
        return TOUCH_TAP;

    if (seq[0] == seq[1] && idx == 2)
        return TOUCH_DOUBLE_TAP;

    return TOUCH_UNKNOWN;
}

TouchGesture
TouchSequence::getGesture()
{
#if defined(DEBUG_TOUCH)
    Serial.print("gesture: ");
    for (int i = 0; i < MAX_TOUCH_SEQ; ++i) {
        Serial.print(seq[i]);
        Serial.print(' ');
    }
    Serial.println();
#endif

    TouchGesture gesture;
    if ((gesture = checkShortSwipe()) != TOUCH_UNKNOWN)
        return gesture;

    if ((gesture = checkLongSwipe()) != TOUCH_UNKNOWN)
        return gesture;

    if ((gesture = checkTap()) != TOUCH_UNKNOWN)
        return gesture;

    if (seq[0] == 0xFF && proximityEvent)
        return TOUCH_PROXIMITY;

    return TOUCH_UNKNOWN;
}

void
TouchSequence::enableInterrupt()
{
    if (interruptPin)
        attachInterrupt(interruptPin, wakeUpInt1, LOW);
    else
        attachInterrupt(interruptPin, wakeUpInt0, LOW);
}

bool
TouchSequence::isInterrupted()
{
    return digitalRead(interruptPin ? 3 : 2) == 0;
}

bool
TouchSequence::isTouched()
{
    return mpr121.touched.all & 0x1FF;
}

bool
TouchSequence::isTouched(byte electrode)
{
    return mpr121.touched.all & (1 << (int)electrode);
}

bool
TouchSequence::isProximity()
{
    return proximityEvent && mpr121.touched.eleprox;
}

byte
TouchSequence::getLastTouch()
{
    if (idx)
        return seq[idx - 1];
    return seq[0];
}
