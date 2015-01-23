#include <EEPROM.h>
#include <Arduino.h>  // for type definitions

template <class T>
int EEPROM_writeAnything(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
        EEPROM.write(ee++, *p++);
    return i;
}

template <class T>
int EEPROM_readAnything(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
        *p++ = EEPROM.read(ee++);
    return i;
}

template <class T>
int loadSettings(int offset, T &dest)
{
    T settings;
    if (EEPROM_readAnything(offset, settings) == sizeof(T))
        dest = settings;
    else
        Serial.println("failed to load settings");
}

template <class T>
void writeSettings(int offset, T &src)
{
    return EEPROM_writeAnything(offset, src);
}
