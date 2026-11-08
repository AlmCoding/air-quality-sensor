#pragma once

#include "Display.h"
#include <SensirionI2cScd4x.h>
#include <cstdint>

class Sampler {
public:
  bool init();
  void run();

private:
  static constexpr bool kScd41Enabled = true;

  static constexpr uint32_t kSampleIntervalSeconds = 60;
  static constexpr uint32_t kSampleIntervalMillis =
      kSampleIntervalSeconds * 1000UL;
  static constexpr uint32_t kSampleStoreReductionFactor =
      5; // Store every 5th sample
  static constexpr uint32_t kDataReadyCheckIntervalMillis = 100;

  bool isSampleDue() const;
  bool isDataReady();
  bool readMeasurement(uint16_t &co2Ppm, float &tempC, float &rhPct);
  void recordSampleTime();
  void storeSample(uint16_t co2Ppm, float tempC, float rhPct,
                   uint32_t windowOpenSeconds);
  void updateDisplayedTime();

  SensirionI2cScd4x sensor_;
  bool initialized_ = false;
  bool hasSampled_ = false;
  bool lastSampleHadUnixTime_ = false;
  bool hasCheckedDataReady_ = false;
  uint32_t lastSampleMillis_ = 0;
  uint32_t lastSampleUnixTime_ = 0;
  uint32_t lastDataReadyCheckMillis_ = 0;
  uint32_t lastDisplayedMinute_ = 0;
  bool hasDisplayedTime_ = false;
  bool windowOpen_ = false;
  uint32_t windowOpenSeconds_ = 0;
  uint32_t windowOpenStartMillis_ = 0;
  uint32_t sampleCounter_ =
      kSampleStoreReductionFactor - 1; // Start at max to store first sample
};
