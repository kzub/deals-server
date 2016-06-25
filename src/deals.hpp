#ifndef SRC_DEALS_HPP
#define SRC_DEALS_HPP

#include "shared_memory.hpp"
#include "utils.hpp"

namespace deals {

#define DEALS_EXPIRES 3600

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

struct DateInterval {
  uint32_t from;
  uint32_t to;
};

struct StayInterval {
  uint8_t from;
  uint8_t to;
};

namespace utils {
union PlaceCodec {
  uint32_t int_code;
  char iata_code[4];
};

void copy(i::DealInfo& dst, const i::DealInfo& src);
uint16_t get_max_price_in_array(i::DealInfo*& dst, uint16_t size);

uint32_t origin_to_code(std::string code);
std::string code_to_origin(uint32_t code);

void print(const i::DealInfo& deal);
void print(const DealInfo& deal);
std::string sprint(const DealInfo& deal);

uint32_t date_to_int(std::string date);
std::string int_to_date(uint32_t date);

bool check_destinations_format(std::string destinations);
bool check_weekdays_format(std::string weekdays);
bool check_date_format(std::string date);
bool check_date_to_date(std::string date_from, std::string date_to);
};

class DealsDatabase {
 public:
  DealsDatabase();
  ~DealsDatabase();

  void addDeal(std::string origin, std::string destination, std::string departure_date,
               std::string return_date, bool direct_flight, uint32_t price, std::string data);

  std::vector<DealInfo> searchForCheapestEver(
      std::string origin, std::string destinations, std::string departure_date_from,
      std::string departure_date_to, std::string departure_days_of_week,
      std::string return_date_from, std::string return_date_to, std::string return_days_of_week,
      uint16_t stay_from, uint16_t stay_to, bool direct_flights, bool stops_flights, uint16_t limit,
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
        filter_flight_by_stops(false),
        filter_departure_weekdays(false),
        filter_return_weekdays(false),
        filter_stay_days(false),
        filter_limit(20),
        query_is_broken(false) {
  }

  void origin(std::string origin);
  void destinations(std::string destinations);
  void departure_dates(std::string departure_date_from, std::string departure_date_to);
  void return_dates(std::string return_date_from, std::string return_date_to);
  void direct_flights(bool direct_flights, bool stops_flights);
  void max_lifetime_sec(uint32_t max_lifetime);
  void deals_limit(uint16_t limit);
  void stay_days(uint16_t stay_from, uint16_t stay_to);
  void departure_weekdays(std::string days_of_week);
  void return_weekdays(std::string days_of_week);

 protected:
  std::vector<i::DealInfo> exec();

  // before iteration
  void pre_process_function();

  /* function that will be called by TableProcessor
        *  for iterating over all not expired pages in table */
  bool process_function(i::DealInfo* elements, uint32_t size);

  // after iteration
  void post_process_function();
  uint8_t weekdays_bitmask(std::string days_of_week);

  friend class DealsDatabase;

 private:
  shared_mem::Table<i::DealInfo>& table;
  std::vector<i::DealInfo> matched_deals;

  bool filter_origin;
  uint32_t origin_value;

  bool filter_destination;
  std::vector<uint32_t> destination_values_vector;

  bool filter_departure_date;
  DateInterval departure_date_values;

  bool filter_return_date;
  DateInterval return_date_values;

  bool filter_timestamp;
  uint32_t timestamp_value;

  bool filter_flight_by_stops;
  bool direct_flights_flag;
  bool stops_flights_flag;

  bool filter_departure_weekdays;
  uint8_t departure_weekdays_bitmask;

  bool filter_return_weekdays;
  uint8_t return_weekdays_bitmask;

  bool filter_stay_days;
  StayInterval stay_days_values;

  uint16_t filter_limit;
  uint16_t deals_slots_used;
  uint16_t max_price_deal;
  bool query_is_broken;

  // this pointers used at search for speed optimization
  // for iterating throught simple values array but not vectors.
  // at exec() function there are local arrays this pointers
  // will point to
  uint32_t* destination_values;  // size = filter_limit
  i::DealInfo* result_deals;     // size = filter_limit
};
}

#endif