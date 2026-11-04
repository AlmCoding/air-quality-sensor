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

namespace {

constexpr bool kScd41Enabled = false;

} // namespace

bool Sampler::init() {
  if (kScd41Enabled) {
    sensor_.begin(Wire, SCD41_I2C_ADDR_62);

    const int16_t error = sensor_.startPeriodicMeasurement();
    if (error != 0) {
      DEBUG_ERROR("SCD41 startPeriodicMeasurement failed: %d", error);
      return false;
    }
  }

  DEBUG_INFO("Sampler initialized successfully!");
  initialized_ = true;
  return true;
}

void Sampler::run() {
  if (!initialized_) {
    return;
  }

  if (!isSampleDue() || (kScd41Enabled && !isDataReady())) {
    return;
  }

  updateDisplayedTime();

  DEBUG_INFO("Sampling air quality data ...");
  bool windowOpen = false;
  uint16_t co2Ppm = 0;
  float tempC = 0.0f;
  float rhPct = 0.0f;
  if (kScd41Enabled && !readMeasurement(co2Ppm, tempC, rhPct)) {
    return;
  }
  recordSampleTime();
  Display::instance().drawSample({co2Ppm, tempC, rhPct, windowOpen});

  if (!kScd41Enabled) {
    return;
  }

  storeSample(co2Ppm, tempC, rhPct, windowOpen);
}

void Sampler::storeSample(uint16_t co2Ppm, float tempC, float rhPct,
                          bool windowOpen) {
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
    DEBUG_INFO("DataStore file name set to: %s", fileName);
  }

  uint32_t unixTimestamp = 0;
  if (!Clock::instance().getUnixTime(unixTimestamp)) {
    DEBUG_ERROR("Failed to get Unix time from Clock!");
    return;
  }

  if (!dataStore.addSample(co2Ppm, tempC, rhPct, windowOpen, unixTimestamp)) {
    DEBUG_ERROR("Failed to store SCD41 sample!");
  }
}

void Sampler::updateDisplayedTime() {
  Clock &clock = Clock::instance();
  if (!clock.isSet()) {
    if (!hasDisplayedTime_) {
      Display::instance().drawTime({});
      hasDisplayedTime_ = true;
    }
    return;
  }

  uint32_t currentUnixTime = 0;
  if (!clock.getUnixTime(currentUnixTime)) {
    DEBUG_ERROR("Failed to get clock time for display update!");
    return;
  }

  const uint32_t currentMinute = currentUnixTime / 60;
  if (!hasDisplayedTime_ || currentMinute != lastDisplayedMinute_) {
    DateTime dateTime = {};
    if (!clock.getDateTime(dateTime)) {
      DEBUG_ERROR("Failed to get date/time for display update!");
      return;
    }

    DEBUG_INFO("Update displayed time: %02u:%02u", //
               (currentMinute / 60) % 24, currentMinute % 60);
    Display::instance().drawTime({dateTime.year, dateTime.month, dateTime.day,
                                  dateTime.hour, dateTime.minute, true});
    lastDisplayedMinute_ = currentMinute;
    hasDisplayedTime_ = true;
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

  DEBUG_INFO("SCD41 measurement: CO2=%u ppm, Temp=%.2f C, RH=%.2f %%", //
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
