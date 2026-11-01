#include <Arduino.h>

#include "DataStore.h"
#include "Sampler.h"

namespace {

Sampler sampler;

} // namespace

void setup() {
  Serial.begin(921600);

  if (!sampler.init()) {
    Serial.println("Failed to initialize sampler");
  }
}

void loop() { sampler.run(); }