#include <sys/time.h>
#include <iostream>
#include <vector>

#include <algorithm>
#include <map>

#include <cassert>
// #include <cinttypes>
// #include <climits>

typedef long long timing_t;
timing_t get_timestamp_us() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  timing_t time_in_mill = (tv.tv_sec) * 1000000 + (tv.tv_usec);

  return time_in_mill;
}

struct DstInfo {
  uint32_t counter;
  uint32_t destination;
};

uint32_t values[] = {1000, 2000, 2000, 2000, 2000, 3000, 3000, 4000};
uint32_t getRandomInt(uint32_t min = 0) {
  uint32_t value1 = rand() & 0x00000FFF;
  uint32_t value2 = rand() & 0x00000007;
  uint32_t value = value1 < values[value2] ? values[value2] : value1;
  value += min;

  if (value < min) {
    std::cout << "ALARM!! " << min << " " << value << std::endl;
    return min;
  }
  return value;
}

std::vector<DstInfo> alg1(std::vector<DstInfo>& destinations, uint32_t filter_limit) {
  std::vector<DstInfo> top_destinations;
  // group 1
  //-----------------------------------------
  for (auto& current_element : destinations) {
    bool found = false;
    for (auto& dst : top_destinations) {
      if (dst.destination == current_element.destination) {
        dst.counter++;
        found = true;
        break;
      }
    }
    if (!found) {
      current_element.counter = 1;
      top_destinations.push_back(current_element);
    }
  }

  std::sort(top_destinations.begin(), top_destinations.end(),
            [](const DstInfo& a, const DstInfo& b) { return a.counter > b.counter; });

  if (top_destinations.size() > filter_limit) {
    top_destinations.resize(filter_limit);
  }

  return top_destinations;
}

std::vector<DstInfo> alg2(std::vector<DstInfo>& destinations, uint32_t filter_limit) {
  std::vector<DstInfo> top_destinations;

  std::map<uint32_t, uint32_t> grouped;
  for (auto& current_element : destinations) {
    grouped[current_element.destination]++;
  }

  for (auto& v : grouped) {
    top_destinations.push_back({v.second, v.first});
  }

  std::sort(top_destinations.begin(), top_destinations.end(),
            [](const DstInfo& a, const DstInfo& b) { return a.counter > b.counter; });

  if (top_destinations.size() > filter_limit) {
    top_destinations.resize(filter_limit);
  }

  return top_destinations;
}

int main() {
  std::vector<DstInfo> destinations;
  int filter_limit = 1000;

  for (uint32_t i = 0; i < 1000000; i++) {
    destinations.push_back({0, getRandomInt()});
  }

  timing_t start = get_timestamp_us();
  std::vector<DstInfo> result1 = alg1(destinations, filter_limit);
  timing_t end = get_timestamp_us();
  std::cout << "grouping 1 time:" << end - start << std::endl;

  start = get_timestamp_us();
  std::vector<DstInfo> result2 = alg2(destinations, filter_limit);
  end = get_timestamp_us();
  std::cout << "grouping 2 time:" << end - start << std::endl;

  int c = 0;
  for (auto dst : result1) {
    std::cout << dst.destination << ":" << dst.counter << " " << result2[c].destination << ":"
              << result2[c].counter << std::endl;
    assert(dst.destination == result2[c].destination);
    c++;
  }
  std::cout << "EQUAL" << std::endl;
  // printing
  //-----------------------------------------
  for (auto dst : result1) {
    std::cout << dst.destination << ":" << dst.counter << std::endl;
  }
  return 0;
}
