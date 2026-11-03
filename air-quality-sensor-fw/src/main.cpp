#include <Arduino.h>
#include <Wire.h>

#include "Connectivity.h"
#include "DataStore.h"
#include "Display.h"
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
  setCpuFrequencyMhz(160);
  Serial.begin(921600);

#if (ENABLE_DEBUG_OUTPUT == 1)
  delay(3000);
  Serial.begin(921600);
  while (!Serial) {
    delay(100);
  }
#endif

  DEBUG_INFO(DIVIDER);
  DEBUG_INFO("Setup Air Quality Sensor [...]");
  DEBUG_INFO("Firmware version: %s", FIRMWARE_VERSION);
  DEBUG_INFO("Free heap: %u bytes", ESP.getFreeHeap());
  DEBUG_INFO("CPU frequency: %d MHz", getCpuFrequencyMhz());

  Wire.begin();

  if (!Display::instance().init()) {
    DEBUG_ERROR("SSD1306 display initialization failed!");
  }

  if (!Connectivity::instance().init()) {
    DEBUG_ERROR("Failed to initialize connectivity!");
  }

  if (!sampler.init()) {
    DEBUG_ERROR("Failed to initialize sampler!");
  }

  DEBUG_INFO("Setup Air Quality Sensor [OK]");
}

void loop() {
  Connectivity::instance().run();
  sampler.run();
}
