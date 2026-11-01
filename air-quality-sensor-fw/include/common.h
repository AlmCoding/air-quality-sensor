#pragma once

#include <Arduino.h>

#define ENABLE_DEBUG_OUTPUT 1
constexpr char FIRMWARE_VERSION[] = "V0.0.1";
constexpr char DIVIDER[] = "<=====================================>";

#if ENABLE_DEBUG_OUTPUT == 1
void debugPrint(const char *prefix, const char *fmt, ...);
#endif

#if ENABLE_DEBUG_OUTPUT == 1
#define ASSERT(condition)                                                      \
  do {                                                                         \
    if (!(condition)) {                                                        \
      Serial.print(F("ASSERT FAILED: "));                                      \
      Serial.print(F(__FILENAME__));                                           \
      Serial.print(F(":"));                                                    \
      Serial.print(__LINE__);                                                  \
      Serial.print(F(" => "));                                                 \
      Serial.println(F(#condition));                                           \
      while (true) {                                                           \
        delay(100);                                                            \
      }                                                                        \
    }                                                                          \
  } while (0)
#else
#define ASSERT(condition)
#endif
