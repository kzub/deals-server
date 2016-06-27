#include "timing.hpp"
#include <unistd.h>

namespace timing {

//---------------------------------------------------
timing_t getTimestampMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  timing_t time_in_mill = debug_time_shift + (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;

  return time_in_mill;
}

//---------------------------------------------------
uint32_t getTimestampSec() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return debug_time_shift + tv.tv_sec;
}

//---------------------------------------------------
// TIMER
//---------------------------------------------------
Timer::Timer(const char *_name) : name(_name) {
  tick_t now;
  now.clock = getTimestampMs();
  ticks.push_back(now);
}

//---------------------------------------------------
void Timer::tick(const char *text) {
  tick_t now;
  if (text != nullptr) {
    now.text = text;
  }
  now.clock = getTimestampMs();
  ticks.push_back(now);
  printLastTick();
}

//---------------------------------------------------
void Timer::finish(const char *text) {
  tick_t now;
  if (text != nullptr) {
    now.text = text;
  }
  now.clock = getTimestampMs();
  ticks.push_back(now);
  printLastTick();
  report();
}

//---------------------------------------------------
void Timer::printLastTick() {
  timing_t totalms = ticks.back().clock - ticks[ticks.size() - 2].clock;
  std::cout << name << " " << totalms << "ms " << ticks.back().text << std::endl;
};

//---------------------------------------------------
void Timer::report() {
  int idx = 1;
  // timing_t clock_last = ticks.begin()->clock;

  std::cout << "--------------------------------------------" << std::endl;

  for (std::vector<tick_t>::iterator it = ticks.begin() + 1; it != ticks.end(); ++it) {
    timing_t msec = it->clock - (it - 1)->clock;
    std::cout << name << " " << idx << ") " << msec << "ms " << it->text << std::endl;
    idx++;
  }

  timing_t totalms = ticks.back().clock - ticks.front().clock;
  std::cout << "--------------------------------------------" << std::endl;
  std::cout << name << " TOTAL: " << totalms << "ms " << std::endl;
  std::cout << "--------------------------------------------" << std::endl;
  std::cout.flush();
}

//---------------------------------------------------
// TIMELORD
//---------------------------------------------------
TimeLord::operator uint32_t() {
  return tick_counter / ticks_in_second;
}

TimeLord &TimeLord::operator++() {
  tick_counter++;

  if (tick_counter % ticks_in_second == 0) {
    debug_time_shift++;
  }

  return *this;
}

//---------------------------------------------------
TimeLord &TimeLord::operator+=(uint32_t step) {
  tick_counter += step;

  if (tick_counter % ticks_in_second == 0) {
    debug_time_shift += step;
  }

  return *this;
}
//---------------------------------------------------
void TimeLord::reset() {
  tick_counter = 1;
}
//---------------------------------------------------
bool TimeLord::test(uint32_t x) {
  return tick_counter <= (x * ticks_in_second);
}

//---------------------------------------------------
void unit_test() {
  Timer timer("TEST");

  timer.tick("stage1");
  timer.tick("stage2");
  usleep(1000000);
  timer.tick("stage3");
  usleep(200000);
  timer.tick("stage4");
  timer.finish("finish");
}
}