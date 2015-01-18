#ifndef DEBUG_H
#define DEBUG_H

#if !defined(NDEBUG)
#include "cpp_magic.h"

#define _DEBUG_PRINT(x)     Serial.print(x);
#define _DEBUG_IT(x, next)  _DEBUG_PRINT(x)
#define _DEBUG_LAST(x)      Serial.println(x);
#define DEBUG(...) \
    do { \
        MAP_SLIDE(_DEBUG_IT, _DEBUG_LAST, EMPTY, __VA_ARGS__) \
    } while(0)
#define DEBUG_(...) do { MAP(_DEBUG_PRINT, EMPTY, __VA_ARGS__) } while(0)
#define DEBUG_FMT(a, b) do { Serial.println(a, b); } while(0)
#define DEBUG_FMT_(a, b) do { Serial.print(a, b); } while(0)

#else

#define DEBUG(...) do { } while (0)
#define DEBUG_(...) do { } while (0)
#define DEBUG_FMT(...) do { } while (0)
#define DEBUG_FMT_(...) do { } while (0)

#endif

#endif // DEBUG_H
