#ifndef SRC_DEALS_TYPES_HPP
#define SRC_DEALS_TYPES_HPP

#include <algorithm>
#include <memory>
#include "shared_memory.hpp"

#define DEALS_EXPIRES 60 * 60 * 24

#define DEALINFO_TABLENAME "DealsInfo"
#define DEALINFO_PAGES 5000
#define DEALINFO_ELEMENTS 10000

#define DEALDATA_TABLENAME "DealsData"
#define DEALDATA_PAGES 10000
#define DEALDATA_ELEMENTS 50000000

namespace deals {
namespace i {
struct DealInfo {
  uint32_t timestamp;
  uint32_t origin;
  uint32_t destination;
  uint32_t departure_date;
  uint32_t return_date;
  uint32_t price;
  uint8_t stay_days;
  uint8_t destination_country;
  uint8_t departure_day_of_week;
  uint8_t return_day_of_week;
  bool direct;
  bool overriden;  // show that not cheapest but lastest in period
  char page_name[MEMPAGE_NAME_MAX_LEN];
  uint32_t index;
  uint32_t size;
};

using DealData = uint8_t;  // aka char
using sharedDealData = shared_mem::ElementExtractor<i::DealData>;
}  // namespace deals::i

struct DealInfoTest {
  std::string origin;             // this data need only for testing
  std::string destination;        // this data need only for testing
  std::string departure_date;     // this data need only for testing
  std::string return_date;        // this data need only for testing
  uint32_t timestamp;             // this data need only for testing
  uint32_t price;                 // this data need only for testing
  uint8_t stay_days;              // this data need only for testing
  uint8_t departure_day_of_week;  // this data need only for testing
  uint8_t return_day_of_week;     // this data need only for testing
  uint8_t destination_country;    // this data need only for testing
  bool direct;                    // this data need only for testing
  bool overriden;                 // this data need only for testing
};

class DealInfo {
 public:
  DealInfo(std::string _data, std::shared_ptr<DealInfoTest> _testing)
      : data(_data), test(_testing) {
  }

  std::string data;
  std::shared_ptr<DealInfoTest> test;
};
}  // namespace deals
#endif