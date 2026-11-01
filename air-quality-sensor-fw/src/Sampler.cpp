#include "Sampler.h"
#include "common.h"

#include "Clock.h"
#include "DataStore.h"

#include <Arduino.h>
#include <Wire.h>

#define DEBUG_ENABLE_SAMPLER 1
#if ((DEBUG_ENABLE_SAMPLER == 1) && (ENABLE_DEBUG_OUTPUT == 1))
#define DEBUG_INFO(f, ...) debugPrint("[INF][Sampler]", f, ##__VA_ARGS__)
#define DEBUG_ERROR(f, ...) debugPrint("[ERR][Sampler]", f, ##__VA_ARGS__)
#else
#define DEBUG_INFO(...)
#define DEBUG_ERROR(...)
#endif

bool Sampler::init() {
  Wire.begin();
  sensor_.begin(Wire, SCD41_I2C_ADDR_62);

  const int16_t error = sensor_.startPeriodicMeasurement();
  if (error != 0) {
    DEBUG_ERROR("SCD41 startPeriodicMeasurement failed: %d\n", error);
    return false;
  }

  DEBUG_INFO("SCD41 initialized successfully\n");
  initialized_ = true;
  return true;
}

void Sampler::run() {
  if (!initialized_) {
    return;
  }

  if (!isSampleDue() || !isDataReady()) {
    return;
  }

  DEBUG_INFO("Sampling SCD41 sensor ...\n");

  uint16_t co2Ppm = 0;
  float tempC = 0.0f;
  float rhPct = 0.0f;
  if (!readMeasurement(co2Ppm, tempC, rhPct)) {
    return;
  }
  recordSampleTime();

  if (!Clock::instance().isSet()) {
    DEBUG_ERROR("Skipping sample storage due to clock not set!");
    return;
  }

  DataStore &dataStore = DataStore::instance();
  if (dataStore.isInErrorState()) {
    DEBUG_ERROR("Skipping sample storage due to DataStore error!");
    return;
  }

  if (dataStore.sampleCount() == 0) {
    int32_t year = 0;
    int32_t weekNumber = 0;

    if (!Clock::instance().getYear(year) ||
        !Clock::instance().getCalendarWeekNumber(weekNumber)) {
      DEBUG_ERROR("Failed to get year or calendar week number from Clock!");
      return;
    }

    char fileName[32] = {};
    std::snprintf(fileName, sizeof(fileName), "log_%04d_%02d.bin", //
                  year, weekNumber);
    if (!dataStore.setFileName(fileName)) {
      DEBUG_ERROR("Failed to set DataStore file name: %s", fileName);
      return;
    }
  }

  if (!dataStore.addSample(co2Ppm, tempC, rhPct, false)) {
    DEBUG_ERROR("Failed to store SCD41 sample!");
  }
}

bool Sampler::isDataReady() {
  const uint32_t currentMillis = millis();
  if (hasCheckedDataReady_ &&
      static_cast<uint32_t>(currentMillis - lastDataReadyCheckMillis_) <
          kDataReadyCheckIntervalMillis) {
    return false;
  }
  hasCheckedDataReady_ = true;
  lastDataReadyCheckMillis_ = currentMillis;

  bool dataReady = false;
  int16_t error = sensor_.getDataReadyStatus(dataReady);
  if (error != 0) {
    DEBUG_ERROR("SCD41 getDataReadyStatus failed: %d\n", error);
    return false;
  }
  return dataReady;
}

bool Sampler::readMeasurement(uint16_t &co2Ppm, float &tempC, float &rhPct) {
  const int16_t error = sensor_.readMeasurement(co2Ppm, tempC, rhPct);
  if (error != 0) {
    DEBUG_ERROR("SCD41 readMeasurement failed: %d\n", error);
    return false;
  }

  DEBUG_INFO("SCD41 measurement: CO2=%u ppm, Temp=%.2f C, RH=%.2f %%\n", //
             co2Ppm, tempC, rhPct);
  return true;
}

bool Sampler::isSampleDue() const {
  if (!hasSampled_) {
    return true;
  }

  Clock &clock = Clock::instance();
  uint32_t currentUnixTime = 0;
  const bool hasCurrentUnixTime =
      clock.isSet() && clock.getUnixTime(currentUnixTime);
  if (hasCurrentUnixTime && lastSampleHadUnixTime_ &&
      currentUnixTime >= lastSampleUnixTime_) {
    return currentUnixTime - lastSampleUnixTime_ >= kSampleIntervalSeconds;
  }

  return static_cast<uint32_t>(millis() - lastSampleMillis_) >=
         kSampleIntervalMillis;
}

void Sampler::recordSampleTime() {
  hasSampled_ = true;
  lastSampleMillis_ = millis();
  lastSampleHadUnixTime_ = Clock::instance().getUnixTime(lastSampleUnixTime_);
}
