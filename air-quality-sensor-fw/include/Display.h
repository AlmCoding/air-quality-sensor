#pragma once

#include <cstdint>

struct SampleData {
  uint16_t co2Ppm;
  float tempC;
  float humidityPct;
  bool windowOpen;
};

class Display {
public:
  bool init();
  void show(const SampleData &sample);

private:
  static constexpr uint8_t kI2cAddress = 0x3C;
};
