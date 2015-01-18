#include "TouchSequence.h"
#include <Wire.h>
#include "MPR121_registers.h"
#include "MPR121_conf.h"
#include "debug.h"

struct MPR121Settings defaultSettings;

// make sure that the MPR121 is stopped when configuring registers
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
            inst->stop();
    }
    void release() {
        if (shouldLock)
            inst->run();
        shouldLock = false;
    }
    bool shouldLock;
    TouchSequence *inst;
};

// Interrupt handlers
static void wakeUpInt0() {
    detachInterrupt(0);
}

static void wakeUpInt1() {
    detachInterrupt(1);
}

// ===============
//  TouchSequence
// ===============

TouchSequence::TouchSequence(byte mpr121Addr, byte interruptPin) :
    interruptPin(interruptPin), idx(0),
    proximityEvent(false), running(false), sleepMode(HIGH_SAMPLING_MODE)
{
    mpr121.address    = mpr121Addr;
    electrodes.top    = ELECTRODE_TOP;
    electrodes.left   = ELECTRODE_LEFT;
    electrodes.bottom = ELECTRODE_BOTTOM;
    electrodes.right  = ELECTRODE_RIGHT;
    electrodes.center = ELECTRODE_CENTER;
}

void
TouchSequence::begin(byte electrodes, byte proximityMode)
{
    DEBUG("starting Wire library");
    Wire.begin();

    mpr121.ele_en = electrodes & 0x0F;
    mpr121.eleprox_en = proximityMode & 0x03;
    mpr121.cl = 2;

    applySettings(defaultSettings);
    setTouchThreshold(defaultSettings.touch);
    setReleaseThreshold(defaultSettings.release);

    // start in run mode
    run();
    clear();

    // turn off auto configure after the initial register values have loaded
    setRegister(ACCR0, 0);
}

void
TouchSequence::dump()
{
    DEBUG("Registers: ");
    for (int i = 0; i < 0x7F; ++i) {
        if (i % 8 == 0)
            DEBUG_("0x0", i, " ");
        DEBUG_FMT_(getRegister(i), HEX);
        if (i % 8 == 7)
            DEBUG();
        else
            DEBUG_(" ");
    }
#if 0
    DEBUG_("Touch status: ");
    DEBUG_FMT_(getRegister(0), HEX);
    DEBUG_(" ");
    DEBUG_FMT(getRegister(1), HEX);

    DEBUG("Filter status: ");
    for (int i = 0x04; i < 0x1E; i += 2) {
        DEBUG_FMT_((int)getRegister(i + 1) << 8 | getRegister(i), DEC);
        DEBUG_(" ");
    }
    DEBUG("");

    DEBUG("Baseline: ");
    for (int i = 0x1E; i < 0x2B; ++i) {
        DEBUG_FMT_(getRegister(i), DEC);
        DEBUG_(" ");
    }
    DEBUG("");

    DEBUG("Filters:");
    for (int i = 0x2B; i < 0x41; ++i) {
        DEBUG_FMT_(getRegister(i), HEX);
        DEBUG_(" ");
    }
    DEBUG("");

    DEBUG("Thresholds:");
    for (int i = 0x41; i < 0x5B; ++i) {
        DEBUG_FMT_(getRegister(i), HEX);
        DEBUG_(" ");
    }
    DEBUG("");

    DEBUG("Conf:");
    for (int i = 0x5B; i < 0x5F; ++i) {
        DEBUG_FMT_(getRegister(i), HEX);
        DEBUG_(" ");
    }
    DEBUG("");

    DEBUG("CDC:");
    for (int i = 0x5F; i < 0x6C; ++i) {
        DEBUG_FMT_(getRegister(i), HEX);
        DEBUG_(" ");
    }
    DEBUG("");

    DEBUG("CDT:");
    for (int i = 0x6C; i < 0x72; ++i) {
        DEBUG_FMT_(getRegister(i), HEX);
        DEBUG_(" ");
    }
    DEBUG("");

    DEBUG("AC:");
    for (int i = 0x7B; i < 0x80; ++i) {
        DEBUG_FMT_(getRegister(i), HEX);
        DEBUG_(" ");
    }
    DEBUG("");
#endif
}

bool
TouchSequence::setRegister(byte reg, byte val)
{
    MPR121ConfigLock lock(this, reg != ECR && (reg < 0x72 || reg > 0x7A));

    DEBUG_("setRegister: 0x");
    DEBUG_FMT_(reg, HEX);

    Wire.beginTransmission(mpr121.address);
    Wire.write(reg);
    Wire.write(val);
    byte errorCode = Wire.endTransmission();

    DEBUG_(" : 0x");
    DEBUG_FMT(val, HEX);

    // automatically update the running flag if ECR is set
    if (errorCode == 0 && reg == ECR) {
        running = val & 0x3F;
        DEBUG("setRegister: ", running ? "running" : "not running");
    }

    return errorCode == 0;
}

byte
TouchSequence::getRegister(byte reg)
{
    Wire.beginTransmission(mpr121.address);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(mpr121.address, (byte)1);
    Wire.endTransmission();
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
TouchSequence::setTouchThreshold(byte val, byte ele)
{
    MPR121ConfigLock lock(this);
    if (ele < 13)
        setRegister(E0TTH + (ele << 1), val);
    else {
        for (int i = 0; i < 13; ++i)
            setRegister(E0TTH + (i << 1), val);
    }
}

void
TouchSequence::setReleaseThreshold(byte val, byte ele)
{
    MPR121ConfigLock lock(this);
    if (ele < 13)
        setRegister(E0RTH + (ele << 1), val);
    else {
        for (int i = 0; i < 13; ++i)
            setRegister(E0RTH + (i << 1), val);
    }
}

void
TouchSequence::applySettings(MPR121Settings &settings)
{
    DEBUG("applySettings:");
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
TouchSequence::sleep()
{
#if 0
    DEBUG("sleep:");
    if (sleepMode == LOW_POWER_MODE)
        return;
    MPR121ConfigLock lock(this);
    //setRegister(AFE1, 0x3F);
    setRegister(AFE2, 0x25);
    sleepMode = LOW_POWER_MODE;
#endif
}

void
TouchSequence::wakeUp()
{
#if 0
    DEBUG("wakeUp:");
    if (sleepMode != LOW_POWER_MODE)
        return;
    MPR121ConfigLock lock(this);
    //setRegister(AFE1, 0xFF);
    setRegister(AFE2, 0x38);
    sleepMode = HIGH_SAMPLING_MODE;
#endif
}

void
TouchSequence::stop()
{
    DEBUG("stop:");
    if (!running)
        return;
    // mask the lower bits to turn proximity/touch electrodes off
    setRegister(ECR, mpr121.ecr & 0xC0);
}

bool
TouchSequence::isRunning()
{
    return running;
}

void
TouchSequence::run()
{
    DEBUG("run:");
    if (running)
        return;
    // restore the current configuration
    setRegister(ECR, mpr121.ecr);
}

bool
TouchSequence::update()
{
    DEBUG("update:");
    int prevTouchedState = mpr121.touched.all;

    // clears the interrupt
    mpr121.touched.status[0] = getRegister(ELE0_7);
    mpr121.touched.status[1] = getRegister(ELE8_PROX);

    // test to see if there was a change
    if (!(prevTouchedState ^ mpr121.touched.all))
        return isTouched();

    for (byte i = 0; i < mpr121.ele_en; ++i) {
        if (isTouched(i)) {
            if (prevTouchedState & (1 << i))
                DEBUG("repeat: ", i);
            else {
                DEBUG("touch: ", i);
                seq[idx++] = i;
                if (idx >= MAX_TOUCH_SEQ)
                    idx = MAX_TOUCH_SEQ;
            }
        }
        else if (prevTouchedState & (1 << i))
            DEBUG("release: ", i);
    }

    if (mpr121.touched.eleprox && seq[0] == 0xFF) {
        proximityEvent = true;
        DEBUG("proximity:");
    }
    else if (seq[0] != 0xFF)
        proximityEvent = false;

    return isTouched();;
}

void
TouchSequence::clear()
{
    DEBUG("clear:");
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
    DEBUG_("gesture: ");
    for (int i = 0; i < MAX_TOUCH_SEQ; ++i) {
        DEBUG_(seq[i], ' ');
    }
    DEBUG("");

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
