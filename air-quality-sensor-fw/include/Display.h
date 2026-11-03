#pragma once

#include <cstdint>

struct SampleData {
  uint16_t co2Ppm;
  float tempC;
  float humidityPct;
  bool windowOpen;
};

struct TimeData {
  int32_t year;
  int32_t month;
  int32_t day;
  int32_t hour;
  int32_t minute;
  bool isSet;
};

class Display {
public:
  static Display &instance();

  bool init();
  void drawTime(const TimeData &time);
  void drawSample(const SampleData &sample);
  void drawSyncing();

private:
  Display() = default;
  Display(const Display &) = delete;
  Display &operator=(const Display &) = delete;

  static constexpr uint8_t kI2cAddress = 0x3C;
};
