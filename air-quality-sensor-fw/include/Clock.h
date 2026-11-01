#pragma once

#include <cstdint>

struct DateTime {
  int32_t year;
  int32_t month;
  int32_t day;
  int32_t hour;
  int32_t minute;
  int32_t second;
};

struct TimeOfDay {
  int32_t hour;
  int32_t minute;
  int32_t second;
};

class Clock {
public:
  static Clock &instance();

  bool setDateTime(const DateTime &dateTime);
  bool getDateTime(DateTime &dateTime) const;
  bool getTimeOfDay(TimeOfDay &timeOfDay) const;
  bool getYear(int32_t &year) const;
  bool getCalendarWeekNumber(int32_t &weekNumber) const;
  bool getUnixTime(uint32_t &unixTime) const;
  bool isSet() const;

private:
  Clock() = default;
  Clock(const Clock &) = delete;
  Clock &operator=(const Clock &) = delete;
};
