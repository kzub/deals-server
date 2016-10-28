#ifndef SRC_DEALS_HPP
#define SRC_DEALS_HPP

#include <memory>
#include <unordered_map>
#include "search_query.hpp"
#include "shared_memory.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace deals {

#define DEALS_EXPIRES 60 * 60 * 12

#define DEALINFO_TABLENAME "DealsInfo"
#define DEALINFO_PAGES 5000
#define DEALINFO_ELEMENTS 10000

#define DEALDATA_TABLENAME "DealsData"
#define DEALDATA_PAGES 10000
#define DEALDATA_ELEMENTS 50000000

void unit_test();

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
using sharedDealData = shared_mem::ElementPointer<i::DealData>;
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

namespace utils {
void print(const i::DealInfo& deal);
void print(const DealInfo& deal);
std::string sprint(const DealInfo& deal);
}  // namespace deals::utils

//------------------------------------------------------------
// DealsDatabase
//------------------------------------------------------------
class DealsDatabase {
 public:
  DealsDatabase();
  ~DealsDatabase();

  void addDeal(const types::Required<types::IATACode>& origin,
               const types::Required<types::IATACode>& destination,
               const types::Required<types::CountryCode>& destination_country,
               const types::Required<types::Date>& departure_date,
               const types::Optional<types::Date>& return_date,
               const types::Required<types::Boolean>& direct_flight,
               const types::Required<types::Number>& price,  //
               const std::string& data);

  // find cheapest by selected filters
  std::vector<DealInfo> searchForCheapest(
      const types::Required<types::IATACode>& origin,
      const types::Optional<types::IATACodes>& destinations,
      const types::Optional<types::CountryCodes>& destination_countries,
      const types::Optional<types::Date>& departure_date_from,
      const types::Optional<types::Date>& departure_date_to,
      const types::Optional<types::Weekdays>& departure_days_of_week,
      const types::Optional<types::Date>& return_date_from,
      const types::Optional<types::Date>& return_date_to,
      const types::Optional<types::Weekdays>& return_days_of_week,
      const types::Optional<types::Number>& stay_from,
      const types::Optional<types::Number>& stay_to,
      const types::Optional<types::Boolean>& direct_flights,
      const types::Optional<types::Number>& limit,
      const types::Optional<types::Number>& max_lifetime_sec,
      const types::Optional<types::Boolean>& roundtrip_flights);

  // find cheapest for each day in provided deparutre interval
  std::vector<DealInfo> searchForCheapestDayByDay(
      const types::Required<types::IATACode>& origin,
      const types::Optional<types::IATACodes>& destinations,
      const types::Optional<types::CountryCodes>& destination_countries,
      const types::Optional<types::Date>& departure_date_from,
      const types::Optional<types::Date>& departure_date_to,
      const types::Optional<types::Weekdays>& departure_days_of_week,
      const types::Optional<types::Date>& return_date_from,
      const types::Optional<types::Date>& return_date_to,
      const types::Optional<types::Weekdays>& return_days_of_week,
      const types::Optional<types::Number>& stay_from,
      const types::Optional<types::Number>& stay_to,
      const types::Optional<types::Boolean>& direct_flights,
      const types::Optional<types::Number>& limit,
      const types::Optional<types::Number>& max_lifetime_sec,
      const types::Optional<types::Boolean>& roundtrip_flights);

  // clear database
  void truncate();

 private:
  // internal <i::DealInfo> contain shared memory page name and
  // information offsets. It's not useful anywhere outside
  // Let's transform internal format to external <DealInfo>
  std::vector<DealInfo> fill_deals_with_data(std::vector<i::DealInfo> i_deals);

  shared_mem::Table<i::DealInfo>* db_index;
  shared_mem::Table<i::DealData>* db_data;

  friend void unit_test();
};

//------------------------------------------------------------
// DealsSearchQuery
//------------------------------------------------------------
class DealsSearchQuery : public shared_mem::TableProcessor<i::DealInfo>, public query::SearchQuery {
 protected:
  DealsSearchQuery(shared_mem::Table<i::DealInfo>& table) : table(table) {
  }
  // preparations and actual processing
  void execute();

  // array size will be equal to filter_result_limit.
  // used for speed optimization, iteration throught vector is slower
  // uint32_t* destination_values = nullptr;  // <- array
  uint16_t result_destinations_count = 0;

 private:
  // function that will be called by TableProcessor
  // for iterating over all not expired pages in table
  void process_element(const i::DealInfo& element) final override;

  // VIRTUAL FUNCTIONS SECTION:
  virtual void process_deal(const i::DealInfo& deal) = 0;
  // if process_element() deside deals worth of processing
  // process_deal() will be called in parent class

  // before and after processing
  virtual void pre_search() = 0;
  virtual void post_search() = 0;

  shared_mem::Table<i::DealInfo>& table;
  uint32_t current_time = 0;
  friend class DealsDatabase;
};

//------------------------------------------------------------
// DealsCheapestByDatesSimple (Simple version of DealsCheapestByPeriod)
//------------------------------------------------------------
class DealsCheapestByDatesSimple : public DealsSearchQuery {
 public:
  DealsCheapestByDatesSimple(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }
  // implement virtual functions:
  void process_deal(const i::DealInfo& deal) final override;
  void pre_search() final override;
  void post_search() final override;

  std::unordered_map<uint32_t, i::DealInfo> grouped_destinations;
  std::vector<i::DealInfo> exec_result;
  uint32_t grouped_max_price = 0;
};

//------------------------------------------------------------
// DealsCheapestDayByDay
//------------------------------------------------------------
class DealsCheapestDayByDay : public DealsSearchQuery {
 public:
  DealsCheapestDayByDay(shared_mem::Table<i::DealInfo>& table) : DealsSearchQuery{table} {
  }

  // implement virtual functions:
  void process_deal(const i::DealInfo& deal) final override;
  void pre_search() final override;
  void post_search() final override;

  std::unordered_map<uint32_t, std::unordered_map<uint32_t, i::DealInfo>>
      grouped_destinations_and_dates;
  std::vector<i::DealInfo> exec_result;
};

}  // namespace deals

#endif