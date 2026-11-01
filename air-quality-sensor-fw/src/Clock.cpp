#include "Clock.h"
#include "common.h"

#include <ctime>
#include <limits>
#include <sys/time.h>

#define DEBUG_ENABLE_CLOCK 1
#if ((DEBUG_ENABLE_CLOCK == 1) && (ENABLE_DEBUG_OUTPUT == 1))
#define DEBUG_INFO(f, ...) debugPrint("[INF][Clock]", f, ##__VA_ARGS__)
#define DEBUG_ERROR(f, ...) debugPrint("[ERR][Clock]", f, ##__VA_ARGS__)
#else
#define DEBUG_INFO(...)
#define DEBUG_ERROR(...)
#endif

namespace {

constexpr time_t kEarliestValidTime = 946080000; // 1999-12-31 00:00:00 UTC

bool isLeapYear(int year) {
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int daysInMonth(int year, int month) {
  constexpr int kDaysPerMonth[] = {31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year)) {
    return 29;
  }
  return kDaysPerMonth[month - 1];
}

int weeksInIsoYear(int year, int januaryFirstWeekday) {
  const int januaryFirstIsoWeekday =
      januaryFirstWeekday == 0 ? 7 : januaryFirstWeekday;
  return januaryFirstIsoWeekday == 4 ||
                 (januaryFirstIsoWeekday == 3 && isLeapYear(year))
             ? 53
             : 52;
}

} // namespace

Clock &Clock::instance() {
  static Clock clock;
  return clock;
}

bool Clock::setDateTime(const DateTime &dateTime) {
  if (dateTime.year < 2000 || dateTime.month < 1 || dateTime.month > 12 ||
      dateTime.day < 1 || dateTime.hour < 0 || dateTime.hour > 23 ||
      dateTime.minute < 0 || dateTime.minute > 59 || dateTime.second < 0 ||
      dateTime.second > 59) {
    return false;
  }

  if (dateTime.day > daysInMonth(dateTime.year, dateTime.month)) {
    return false;
  }

  std::tm timeFields = {};
  timeFields.tm_year = dateTime.year - 1900;
  timeFields.tm_mon = dateTime.month - 1;
  timeFields.tm_mday = dateTime.day;
  timeFields.tm_hour = dateTime.hour;
  timeFields.tm_min = dateTime.minute;
  timeFields.tm_sec = dateTime.second;
  timeFields.tm_isdst = -1;

  const time_t timestamp = std::mktime(&timeFields);
  if (timestamp < kEarliestValidTime) {
    return false;
  }

  timeval value = {};
  value.tv_sec = timestamp;
  value.tv_usec = 0;
  return settimeofday(&value, nullptr) == 0;
}

bool Clock::getDateTime(DateTime &dateTime) const {
  const time_t timestamp = std::time(nullptr);
  if (timestamp < kEarliestValidTime) {
    return false;
  }

  std::tm timeFields = {};
  if (localtime_r(&timestamp, &timeFields) == nullptr) {
    return false;
  }

  dateTime.year = timeFields.tm_year + 1900;
  dateTime.month = timeFields.tm_mon + 1;
  dateTime.day = timeFields.tm_mday;
  dateTime.hour = timeFields.tm_hour;
  dateTime.minute = timeFields.tm_min;
  dateTime.second = timeFields.tm_sec;
  return true;
}

bool Clock::getTimeOfDay(TimeOfDay &timeOfDay) const {
  DateTime dateTime = {};
  if (!getDateTime(dateTime)) {
    return false;
  }

  timeOfDay.hour = dateTime.hour;
  timeOfDay.minute = dateTime.minute;
  timeOfDay.second = dateTime.second;
  return true;
}

bool Clock::getYear(int32_t &year) const {
  DateTime dateTime = {};
  if (!getDateTime(dateTime)) {
    return false;
  }

  year = dateTime.year;
  return true;
}

bool Clock::getCalendarWeekNumber(int32_t &weekNumber) const {
  const time_t timestamp = std::time(nullptr);
  if (timestamp < kEarliestValidTime) {
    return false;
  }

  std::tm timeFields = {};
  if (localtime_r(&timestamp, &timeFields) == nullptr) {
    return false;
  }

  const int32_t weekday = timeFields.tm_wday == 0 ? 7 : timeFields.tm_wday;
  int32_t week = (timeFields.tm_yday + 11 - weekday) / 7;

  const int32_t januaryFirstWeekday =
      (timeFields.tm_wday - timeFields.tm_yday % 7 + 7) % 7;
  const int32_t year = timeFields.tm_year + 1900;
  const int32_t weeksInYear = weeksInIsoYear(year, januaryFirstWeekday);

  if (week < 1) {
    const int32_t previousYear = year - 1;
    const int32_t previousJanuaryFirstWeekday =
        (januaryFirstWeekday - (isLeapYear(previousYear) ? 2 : 1) + 7) % 7;
    week = weeksInIsoYear(previousYear, previousJanuaryFirstWeekday);
  } else if (week > weeksInYear) {
    week = 1;
  }

  weekNumber = week;
  return true;
}

bool Clock::getUnixTime(uint32_t &unixTime) const {
  const time_t timestamp = std::time(nullptr);
  if (timestamp < kEarliestValidTime ||
      static_cast<uint64_t>(timestamp) > std::numeric_limits<uint32_t>::max()) {
    return false;
  }

  unixTime = static_cast<uint32_t>(timestamp);
  return true;
}

bool Clock::isSet() const { return std::time(nullptr) >= kEarliestValidTime; }
