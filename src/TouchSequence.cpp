#include "TouchSequence.h"
#include "MPR121.h"

// #define DEBUG_TOUCH
#define PROXIMITY_ELEC 12

static void wakeUpInt0() {
    detachInterrupt(0);
}

static void wakeUpInt1() {
    detachInterrupt(1);
}

TouchSequence::TouchSequence(byte mpr121Addr, byte interruptPin) :
    mpr121Addr(mpr121Addr), interruptPin(interruptPin), idx(0), touched(false),
    proximityEvent(false)
{
    electrodes.total  = 12;
    electrodes.top    = 0;
    electrodes.left   = 1;
    electrodes.bottom = 2;
    electrodes.right  = 3;
    electrodes.center = 4;
}

void
TouchSequence::begin(byte electrodes, byte proxMode)
{
    if (electrodes <= 12 && electrodes >= 0)
        this->electrodes.total = electrodes;

    if (!MPR121.begin(mpr121Addr)) {
        Serial.print("MPR121 error: ");
        Serial.println(MPR121.getError());
    }
    MPR121_settings_t settings;
    settings.TTHRESH = 10;
    settings.RTHRESH = 5;
    settings.INTERRUPT = interruptPin ? 3 : 2;
    // Baseline tracking enabled
    settings.ECR = 0x80 | (this->electrodes.total & 0x0F) | ((proxMode & 0x03) << 4);
#if defined(DEBUG_TOUCH)
    Serial.print("ECR: ");
    Serial.println(settings.ECR, HEX);
#endif
    MPR121.applySettings(&settings);

    // proximity threshold settings
    if (proxMode) {
        MPR121.setReleaseThreshold(PROXIMITY_ELEC, 1);
        MPR121.setTouchThreshold(PROXIMITY_ELEC, 2);
    }

    clear();
}

void
TouchSequence::setElectrodes(struct Electrodes &electrodes)
{
    this->electrodes.total  = electrodes.total;
    this->electrodes.top    = electrodes.top;
    this->electrodes.left   = electrodes.left;
    this->electrodes.bottom = electrodes.bottom;
    this->electrodes.right  = electrodes.right;
    this->electrodes.center = electrodes.center;
}

bool
TouchSequence::checkRepeatMode()
{
    touched = MPR121.getTouchData(seq[idx - 1]);
    // proximity = MPR121.getTouchData(PROXIMITY_ELEC);
#if defined(DEBUG_TOUCH)
    Serial.print("repeat: ");
    Serial.println(touched ? "touch" : "no touch");
    // Serial.println(proximity ? "proximity" : "no proximity");
#endif
    return touched;
}

bool
TouchSequence::update()
{
    bool interrupted = isInterrupted();

#if defined(DEBUG_TOUCH)
    Serial.println("updating sensors:");
#endif
    // clears the interrupt
    MPR121.updateTouchData();

    if (idx > 0 && !interrupted)
        touched = checkRepeatMode();
    else {
        touched = false;
        for (byte i = 0; i < electrodes.total; ++i) {
            if (!MPR121.getTouchData(i))
                continue;
#if defined(DEBUG_TOUCH)
            Serial.print("touch: ");
            Serial.println(i, DEC);
#endif
            if (MPR121.isNewTouch(i)) {
                seq[idx++] = i;
                if (idx >= MAX_TOUCH_SEQ)
                    idx = MAX_TOUCH_SEQ;
            }
            touched = true;
        }
    }

    proximity = MPR121.getTouchData(PROXIMITY_ELEC);
    if (proximity && seq[0] == 0xFF)
        proximityEvent = true;
    else if (seq[0] != 0xFF)
        proximityEvent = false;

    return touched;
}

void
TouchSequence::clear()
{
    for (int i = 0; i < MAX_TOUCH_SEQ; ++i)
        seq[i] = 0xFF;
    idx = 0;
    touched = false;
    proximity = false;
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
    return touched;
}

bool
TouchSequence::isProximity()
{
    return proximity;
}

byte
TouchSequence::getLastTouch()
{
    if (idx)
        return seq[idx - 1];
    return seq[0];
}
