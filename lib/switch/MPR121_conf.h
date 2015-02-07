#ifndef MPR121_CONF_H
#define MPR121_CONF_H

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
    //   electrodes:    number of electrodes to enable (1-12, 0 = disabled)
    byte electrodes;
    //   proximityMode: 0 = disabled
    //                  1 = electrodes 0-1
    //                  2 = electrodes 0-3
    //                  3 = electrodes 0-11
    byte proximityMode;

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

    // initial threshold/release settings
    byte touch;
    byte release;

    MPR121Settings() :
        electrodes(5),
        proximityMode(0),
        debounce(0x11),
        afe1(0x3F),
        afe2(0x24),
        accr0(0x3F),
        accr1(0x00),
        usl(200),
        lsl(100),
        tl(180),
        touch(5),
        release(2)
    {
        electrode.rising  = (MPR121Filter){ 0x3F, 0x3F, 0x05, 0x00 };
        electrode.falling = (MPR121Filter){ 0x01, 0x3F, 0x10, 0x03 };
        electrode.touched = (MPR121Filter){ 0x00, 0x01, 0x01, 0xFF };
        proximity.rising  = (MPR121Filter){ 0x0F, 0x0F, 0x00, 0x00 };
        proximity.falling = (MPR121Filter){ 0x01, 0x01, 0xFF, 0xFF };
        proximity.touched = (MPR121Filter){ 0x00, 0x00, 0x00, 0x00 };
    }
};

#endif // MPR121_CONF_H
