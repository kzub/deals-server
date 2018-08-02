#ifndef SRC_TIMING_HPP
#define SRC_TIMING_HPP

#include <sys/time.h>
#include <iostream>
#include <vector>

namespace timing {
//---------------------------------------------------
using timing_t = long long;
static uint32_t debug_time_shift = 0;

//---------------------------------------------------
timing_t getTimestampMs();
uint32_t getTimestampSec();

//---------------------------------------------------
// Time measurment tool
//---------------------------------------------------
class Timer {
 public:
  Timer(const char *_name);

  void tick(const char *text = nullptr);
  void printLastTick();
  void finish(const char *text = nullptr);
  void report();

 private:
  using tick_t = struct {
    std::string text;
    timing_t clock;
  };

  std::vector<tick_t> ticks;
  std::string name;
};

//---------------------------------------------------
// Time traveling tool
//---------------------------------------------------
class TimeLord {
 public:
  TimeLord(uint32_t ticks_in_second = 1) : tick_counter(1), ticks_in_second(ticks_in_second) {
  }

  operator uint32_t();
  TimeLord &operator++();
  TimeLord &operator-=(uint32_t step);
  TimeLord &operator+=(uint32_t step);
  void reset();
  bool test(uint32_t x);

 private:
  uint32_t tick_counter;
  const uint32_t ticks_in_second;
};

void unit_test();
}  // namespace timing
#endif