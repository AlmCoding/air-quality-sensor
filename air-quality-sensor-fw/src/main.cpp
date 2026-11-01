#include <Arduino.h>

#include "DataStore.h"
#include "Sampler.h"
#include "common.h"

#define DEBUG_ENABLE_MAIN 1
#if ((DEBUG_ENABLE_MAIN == 1) && (ENABLE_DEBUG_OUTPUT == 1))
#define DEBUG_INFO(f, ...) debugPrint("[INF][Main]", f, ##__VA_ARGS__)
#define DEBUG_ERROR(f, ...) debugPrint("[ERR][Main]", f, ##__VA_ARGS__)
#else
#define DEBUG_INFO(...)
#define DEBUG_ERROR(...)
#endif

namespace {

Sampler sampler;

} // namespace

void setup() {
  Serial.begin(921600);

#if (ENABLE_DEBUG_OUTPUT == 1)
  delay(3000);
  Serial.begin(921600);
  while (!Serial) {
    delay(100);
  }
#endif

  if (!sampler.init()) {
    DEBUG_ERROR("Failed to initialize sampler!");
  }
}

void loop() { sampler.run(); }
