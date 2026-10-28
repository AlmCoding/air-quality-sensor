#pragma once

#include <cstdint>

struct DateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

struct TimeOfDay {
  int hour;
  int minute;
  int second;
};

class Clock {
 public:
  static Clock& instance();

  bool setDateTime(const DateTime& dateTime);
  bool getDateTime(DateTime& dateTime) const;
  bool getTimeOfDay(TimeOfDay& timeOfDay) const;
  bool getUnixTime(uint32_t& unixTime) const;
  bool isSet() const;

 private:
  Clock() = default;
  Clock(const Clock&) = delete;
  Clock& operator=(const Clock&) = delete;
};
