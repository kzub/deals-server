#include <sys/time.h>
#include <iostream>
#include <vector>

typedef long long timing_t;
timing_t get_timestamp_us() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  timing_t time_in_us = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;

  return time_in_us;
}

int main() {
  std::vector<int> test;

  for (int i = 0; i < 100000000; ++i) {
    test.push_back(i);
  }

  int result = 0;
  int start = get_timestamp_us();
  for (auto val : test) {
    result += val;
  }
  int end = get_timestamp_us();
  std::cout << "time auto:" << end - start << " " << result << std::endl;

  result = 0;
  start = get_timestamp_us();
  for (auto& val : test) {
    result += val;
  }
  end = get_timestamp_us();
  std::cout << "time auto&:" << end - start << " " << result << std::endl;

  result = 0;
  start = get_timestamp_us();
  for (const auto val : test) {
    result += val;
  }
  end = get_timestamp_us();
  std::cout << "time const auto:" << end - start << " " << result << std::endl;

  result = 0;
  start = get_timestamp_us();
  for (const auto& val : test) {
    result += val;
  }
  end = get_timestamp_us();
  std::cout << "time const auto&:" << end - start << " " << result << std::endl;

  return 0;
}