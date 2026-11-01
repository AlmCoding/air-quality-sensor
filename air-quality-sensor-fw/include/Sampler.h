#pragma once

#include <SensirionI2cScd4x.h>

#include <cstdint>

class Sampler {
public:
  bool init();
  void run();

private:
  static constexpr uint32_t kSampleIntervalSeconds = 6 * 60;
  static constexpr uint32_t kSampleIntervalMillis =
      kSampleIntervalSeconds * 1000UL;
  static constexpr uint32_t kDataReadyCheckIntervalMillis = 100;

  bool isSampleDue() const;
  bool isDataReady();
  bool readMeasurement(uint16_t &co2Ppm, float &tempC, float &rhPct);
  void recordSampleTime();

  SensirionI2cScd4x sensor_;
  bool initialized_ = false;
  bool hasSampled_ = false;
  bool lastSampleHadUnixTime_ = false;
  bool hasCheckedDataReady_ = false;
  uint32_t lastSampleMillis_ = 0;
  uint32_t lastSampleUnixTime_ = 0;
  uint32_t lastDataReadyCheckMillis_ = 0;
};
