#ifndef SRC_DEALS_HPP
#define SRC_DEALS_HPP

#include "search_query.hpp"
#include "shared_memory.hpp"
#include "utils.hpp"

namespace deals {

//                    12h
// #define DEALS_EXPIRES 43200
#define DEALS_EXPIRES 3600

#define DEALINFO_TABLENAME "DealsInfo"
#define DEALINFO_PAGES 5000
#define DEALINFO_ELEMENTS 10000

#define DEALDATA_TABLENAME "DealsData"
#define DEALDATA_PAGES 10000
#define DEALDATA_ELEMENTS 50000000

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

namespace utils {
void copy(i::DealInfo& dst, const i::DealInfo& src);
uint16_t get_max_price_in_array(i::DealInfo*& dst, uint16_t size);
uint16_t get_max_price_in_pointers_array(i::DealInfo* dst[], uint16_t size);

void print(const i::DealInfo& deal);
void print(const DealInfo& deal);
std::string sprint(const DealInfo& deal);
};

//------------------------------------------------------------
// DealsDatabase
//------------------------------------------------------------
class DealsDatabase {
 public:
  DealsDatabase();
  ~DealsDatabase();

  bool addDeal(std::string origin, std::string destination, std::string departure_date,
               std::string return_date, bool direct_flight, uint32_t price, std::string data);

  std::vector<DealInfo> searchForCheapestEver(
      std::string origin, std::string destinations, std::string departure_date_from,
      std::string departure_date_to, std::string departure_days_of_week,
      std::string return_date_from, std::string return_date_to, std::string return_days_of_week,
      uint16_t stay_from, uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from,
      uint32_t price_to, uint16_t limit, uint32_t max_lifetime_sec,
      ::utils::Threelean roundtrip_flights);

  std::vector<DealInfo> searchForCheapestDayByDay(
      std::string origin, std::string destinations, std::string departure_date_from,
      std::string departure_date_to, std::string departure_days_of_week,
      std::string return_date_from, std::string return_date_to, std::string return_days_of_week,
      uint16_t stay_from, uint16_t stay_to, ::utils::Threelean direct_flights, uint32_t price_from,
      uint32_t price_to, uint16_t limit, uint32_t max_lifetime_sec,
      ::utils::Threelean roundtrip_flights);

  void truncate();

 private:
  std::vector<DealInfo> fill_deals_with_data(std::vector<i::DealInfo> i_deals);

  shared_mem::Table<i::DealInfo>* db_index;
  shared_mem::Table<i::DealData>* db_data;

  friend void unit_test();
};

//------------------------------------------------------------
// DealsSearchQuery
//------------------------------------------------------------
class DealsSearchQuery : public shared_mem::TableProcessor<i::DealInfo>, public query::SearchQuery {
 public:
  ~DealsSearchQuery();

 protected:
  DealsSearchQuery(shared_mem::Table<i::DealInfo>& table) : table(table) {
  }
  // preparations and actual processing
  void execute();

  // array size will be equal to filter_limit.
  // used for speed optimization, iteration throught vector is slower
  uint32_t* destination_values = nullptr;  // <- array
  uint16_t destination_values_size = 0;

 private:
  // function that will be called by TableProcessor
  // for iterating over all not expired pages in table */
  bool process_function(i::DealInfo* elements, uint32_t size);

  // VIRTUAL FUNCTIONS SECTION:
  virtual bool process_deal(const i::DealInfo& deal) = 0;
  // if process_function() deside deals worth of processing
  // process_deal() will be called in parent class

  // before and after processing
  virtual void pre_search() = 0;
  virtual void post_search() = 0;

  shared_mem::Table<i::DealInfo>& table;
  friend class DealsDatabase;
};

//------------------------------------------------------------
// DealsCheapestByPeriod
//------------------------------------------------------------
class DealsCheapestByPeriod : public DealsSearchQuery {
 public:
  DealsCheapestByPeriod(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }
  ~DealsCheapestByPeriod();

  // implement virtual functions:
  bool process_deal(const i::DealInfo& deal);
  void pre_search();
  void post_search();

  uint16_t deals_slots_used;
  uint16_t max_price_deal;

  std::vector<i::DealInfo> exec_result;
  i::DealInfo* result_deals = nullptr;  // size = filter_limit
  // this pointer used at search for speed optimization
  // for iterating throught simple values array but not vectors.
  // at exec() function there be local arrays this pointers
  // will point to
};

//------------------------------------------------------------
// DealsCheapestDayByDay
//------------------------------------------------------------
class DealsCheapestDayByDay : public DealsSearchQuery {
 public:
  DealsCheapestDayByDay(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }
  ~DealsCheapestDayByDay();

  // implement virtual functions:
  bool process_deal(const i::DealInfo& deal);
  void pre_search();
  void post_search();

  // arrays of by date results:
  uint16_t deals_slots_used;
  uint16_t deals_slots_available;
  // uint16_t max_price_deal;

  // std::vector<std::vector<i::DealInfo>> exec_result;
  std::vector<i::DealInfo> exec_result;
  i::DealInfo* result_deals = nullptr;  // size = filter_limit
  // this pointer used at search for speed optimization
  // for iterating throught simple values array but not vectors.
  // at exec() function there be local arrays this pointers
  // will point to
};

}  // namespace deals

#endif