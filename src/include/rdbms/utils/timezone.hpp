#pragma once

#include <memory>
#include <string>

#include <time.h>

namespace rdbms {

// Local time in unspecified timezone.
// A minute is always 60 seconds, no leap seconds.
struct DateTime {
  DateTime() {}
  explicit DateTime(const struct tm&);
  DateTime(int _year, int _month, int _day, int _hour, int _minute, int _second)
      : year(_year),
        month(_month),
        day(_day),
        hour(_hour),
        minute(_minute),
        second(_second) {}

  // "2011-12-31 12:34:56"
  std::string to_iso_string() const;

  int year = 0;    // [1900, 2500]
  int month = 0;   // [1, 12]
  int day = 0;     // [1, 31]
  int hour = 0;    // [0, 23]
  int minute = 0;  // [0, 59]
  int second = 0;  // [0, 59]
};

class TimeZone {
 public:
  TimeZone() = default;
  TimeZone(int east_of_utc, const char* tzname);

  static TimeZone utc();
  // Fixed at GMT+8, no DST
  static TimeZone china();
  static TimeZone load_zone_file(const char* zone_file);

  bool valid() const { return static_cast<bool>(data_); }

  DateTime to_local_time(int64_t seconds_since_epoch,
                         int* utc_offset = nullptr) const;
  int64_t from_local_time(const struct DateTime&,
                          bool post_transition = false) const;

  static struct DateTime to_utc_time(int64_t seconds_since_epoch);
  static int64_t from_utc_time(const DateTime&);

  struct Data;

 private:
  explicit TimeZone(std::unique_ptr<Data> data);
  std::shared_ptr<Data> data_;

  friend class TimeZoneTestPeer;
};

}  // namespace rdbms