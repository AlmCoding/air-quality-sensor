#include "common.h"
#include <cstdio>

void debugPrint(const char *prefix, const char *fmt, ...) {
  uint32_t ms = (uint32_t)millis();
  uint32_t sec = (ms / 1000) & 0x0F; // wrap seconds 0..15
  uint16_t msec = ms % 1000;

  Serial.printf("[%u.%03u] %s: ", sec, msec, prefix);

  va_list ap;
  va_start(ap, fmt);
  char msg[192];
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  Serial.print(msg);

  Serial.print("\n");
}
