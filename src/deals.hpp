#ifndef SRC_DEALS_HPP
#define SRC_DEALS_HPP

#include "search_query.hpp"
#include "shared_memory.hpp"
#include "utils.hpp"

namespace deals {

//                    12h
#define DEALS_EXPIRES 43200

#define DEALINFO_TABLENAME "DealsInfo"
#define DEALINFO_PAGES 1000
#define DEALINFO_ELEMENTS 10000

#define DEALDATA_TABLENAME "DealsData"
#define DEALDATA_PAGES 10000
#define DEALDATA_ELEMENTS 3200000

#define IN_USE 1
#define NOT_IN_USE 0

#define OVERRIDEN_FLAG IN_USE
#define USING(f) ((f == IN_USE) ? 1 : (f == NOT_IN_USE) ? 0 : (f))

void unit_test();

struct Flags {
  bool direct : 1;
  bool overriden : 1;
  uint8_t departure_day_of_week : 4;
  uint8_t return_day_of_week : 4;
  bool is2gds4rt : 1;
};

namespace i {
struct DealInfo {
  uint32_t timestamp;
  uint32_t origin;
  uint32_t destination;
  uint32_t departure_date;
  uint32_t return_date;
  uint8_t stay_days;
  Flags flags;
  uint32_t price;
  char page_name[MEMPAGE_NAME_MAX_LEN];
  uint32_t index;
  uint32_t size;
};
typedef uint8_t DealData;
}

struct DealInfo {
  uint32_t timestamp;
  std::string origin;
  std::string destination;
  std::string departure_date;
  std::string return_date;
  uint8_t stay_days;
  Flags flags;
  uint32_t price;
  std::string data;
};

namespace utils {
void copy(i::DealInfo& dst, const i::DealInfo& src);
uint16_t get_max_price_in_array(i::DealInfo*& dst, uint16_t size);

void print(const i::DealInfo& deal);
void print(const DealInfo& deal);
std::string sprint(const DealInfo& deal);
};

class DealsDatabase {
 public:
  DealsDatabase();
  ~DealsDatabase();

  bool addDeal(std::string origin, std::string destination, std::string departure_date,
               std::string return_date, bool direct_flight, uint32_t price, bool is2gds4rt,
               std::string data);

  std::vector<DealInfo> searchForCheapestEver(
      std::string origin, std::string destinations, std::string departure_date_from,
      std::string departure_date_to, std::string departure_days_of_week,
      std::string return_date_from, std::string return_date_to, std::string return_days_of_week,
      uint16_t stay_from, uint16_t stay_to, bool direct_flights, bool stops_flights,
      bool skip_2gds4rt, uint32_t price_from, uint32_t price_to, uint16_t limit,
      uint32_t max_lifetime_sec);

  void truncate();

 private:
  shared_mem::Table<i::DealInfo>* db_index;
  shared_mem::Table<i::DealData>* db_data;

  friend void unit_test();
};

class DealsSearchQuery : public shared_mem::TableProcessor<i::DealInfo>, public query::SearchQuery {
 public:
  DealsSearchQuery(shared_mem::Table<i::DealInfo>& table) : table(table) {
  }

 protected:
  std::vector<i::DealInfo> exec();

  /* function that will be called by TableProcessor
  *  for iterating over all not expired pages in table */
  bool process_function(i::DealInfo* elements, uint32_t size);
  friend class DealsDatabase;

 private:
  shared_mem::Table<i::DealInfo>& table;
  std::vector<i::DealInfo> matched_deals;
  uint16_t deals_slots_used;
  // this pointers used at search for speed optimization
  // for iterating throught simple values array but not vectors.
  // at exec() function there are local arrays this pointers
  // will point to
  uint32_t* destination_values;  // size = filter_limit
  i::DealInfo* result_deals;     // size = filter_limit
};
}

#endif