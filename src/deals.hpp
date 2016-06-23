#ifndef SRC_DEALS_HPP
#define SRC_DEALS_HPP

#include "shared_memory.hpp"
#include "utils.hpp"

namespace deals {

#define DEALS_EXPIRES 60

#define DEALINFO_TABLENAME "DealsInfo"
#define DEALINFO_PAGES 1000
#define DEALINFO_ELEMENTS 10000

#define DEALDATA_TABLENAME "DealsData"
#define DEALDATA_PAGES 10000
#define DEALDATA_ELEMENTS 3200000

void unit_test();

struct Flags {
  bool direct : 1;
  bool overriden : 1;
};

namespace i {
struct DealInfo {
  uint32_t timestamp;
  uint32_t origin;
  uint32_t destination;
  uint32_t departure_date;
  uint32_t return_date;
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
  Flags flags;
  uint32_t price;
  std::string data;
};

struct DateInterval {
  uint32_t from;
  uint32_t to;
};

namespace utils {
union PlaceCodec {
  uint32_t int_code;
  char iata_code[4];
};

uint32_t origin_to_code(std::string code);
std::string code_to_origin(uint32_t code);
void copy(i::DealInfo& dst, const i::DealInfo& src);
uint16_t get_max_price_in_array(i::DealInfo*& dst, uint16_t size);
void print(const i::DealInfo& deal);
void print(const DealInfo& deal);
std::string sprint(const DealInfo& deal);
uint32_t date_to_int(std::string date);
std::string int_to_date(uint32_t date);
std::string deals_to_json(const DealInfo);
};

class DealsDatabase {
 public:
  DealsDatabase();
  ~DealsDatabase();

  void addDeal(std::string origin, std::string destination,
               std::string departure_date, std::string return_date,
               bool direct_flight, uint32_t price, std::string data);

  std::vector<DealInfo> searchForCheapestEver(
      std::string origin, std::string destinations,
      std::string departure_date_from, std::string departure_date_to,
      std::string return_date_from, std::string return_date_to,
      bool direct_flights, bool stops_flights, uint16_t limit,
      uint32_t max_lifetime_sec);

  void truncate();

 private:
  shared_mem::Table<i::DealInfo>* db_index;
  shared_mem::Table<i::DealData>* db_data;

  friend void unit_test();
};

class DealsSearchQuery : public shared_mem::TableProcessor<i::DealInfo> {
 public:
  DealsSearchQuery(shared_mem::Table<i::DealInfo>& table)
      : table(table),
        filter_origin(false),
        filter_destination(false),
        filter_departure_date(false),
        filter_return_date(false),
        filter_timestamp(false),
        filter_flags(false),
        filter_limit(20) {}

  std::vector<i::DealInfo> exec();

  // before iteration
  void pre_process_function();

  /* function that will be called by TableProcessor
        *  for iterating over all not expired pages in table */
  bool process_function(i::DealInfo* elements, uint32_t size);

  // after iteration
  void post_process_function();

  shared_mem::Table<i::DealInfo>& table;
  std::vector<i::DealInfo> matched_deals;

  void origin(std::string origin);
  void destinations(std::string destinations);
  void departure_dates(std::string departure_date_from,
                       std::string departure_date_to);
  void return_dates(std::string return_date_from, std::string return_date_to);
  void direct_flights(bool direct_flights, bool stops_flights);
  void max_lifetime_sec(uint32_t max_lifetime);
  void deals_limit(uint16_t limit);

  bool filter_origin;
  uint32_t filter_origin_value;

  bool filter_destination;
  uint32_t* filter_destination_values;
  std::vector<uint32_t> filter_destination_values_vector;

  bool filter_departure_date;
  DateInterval filter_departure_date_values;

  bool filter_return_date;
  DateInterval filter_return_date_values;

  bool filter_timestamp;
  uint32_t filter_timestamp_value;

  bool filter_flags;
  bool direct_flights_flag;
  bool stops_flights_flag;

  uint16_t filter_limit;
  uint16_t deals_slots_used;
  i::DealInfo* result_deals;
  uint16_t max_price_deal;
};
}

#endif