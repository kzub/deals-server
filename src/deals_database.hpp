#ifndef SRC_DEALS_HPP
#define SRC_DEALS_HPP

#include <unordered_map>
#include "deals_cheapest.hpp"
#include "deals_cheapest_by_date.hpp"
#include "deals_stats.hpp"
#include "deals_types.hpp"
#include "deals_unique_routes.hpp"
#include "search_query.hpp"
#include "shared_memory.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace deals {

#define TEST_BUILD 0
void unit_test();

//------------------------------------------------------------
// DealsDatabase
//------------------------------------------------------------
class DealsDatabase {
 public:
  DealsDatabase();

  void addDeal(const types::Required<types::IATACode>& origin,
               const types::Required<types::IATACode>& destination,
               const types::Required<types::CountryCode>& destination_country,
               const types::Required<types::Date>& departure_date,
               const types::Optional<types::Date>& return_date,
               const types::Required<types::Boolean>& direct_flight,
               const types::Required<types::Number>& price,  //
               const std::string& data);

  template <typename QueryClass>
  std::vector<DealInfo> searchFor(const types::Required<types::IATACode>& origin,
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
                                  const types::Optional<types::Boolean>& roundtrip_flights,
                                  const types::Optional<types::Date>& departure_or_return_date,
                                  const types::Optional<types::Boolean>& all_combinations);

  const std::string getUniqueRoutesDeals();
  const std::string getStats();

  // clear database
  void truncate();

 private:
  // internal <i::DealInfo> contain shared memory page name and
  // information offsets. It's not useful anywhere outside
  // Let's transform internal format to external <DealInfo>
  std::vector<DealInfo> fill_deals_with_data(std::vector<i::DealInfo> i_deals);

  shared_mem::SharedContext db_context;
  shared_mem::Table<i::DealInfo> db_index;
  shared_mem::Table<i::DealData> db_data;

  friend void unit_test();
};
}  // namespace deals

// ------------------------- IMPLEMENTATIONS -----------------------

namespace deals {
/*---------------------------------------------------------
* DealsDatabase  searchFor
*---------------------------------------------------------*/
template <typename QueryClass>
std::vector<DealInfo> DealsDatabase::searchFor(
    const types::Required<types::IATACode>& origin,
    const types::Optional<types::IATACodes>& destinations,
    const types::Optional<types::CountryCodes>& destination_countries,
    const types::Optional<types::Date>& departure_date_from,
    const types::Optional<types::Date>& departure_date_to,
    const types::Optional<types::Weekdays>& departure_days_of_week,
    const types::Optional<types::Date>& return_date_from,
    const types::Optional<types::Date>& return_date_to,
    const types::Optional<types::Weekdays>& return_days_of_week,
    const types::Optional<types::Number>& stay_from,  //
    const types::Optional<types::Number>& stay_to,
    const types::Optional<types::Boolean>& direct_flights,
    const types::Optional<types::Number>& limit,
    const types::Optional<types::Number>& max_lifetime_sec,
    const types::Optional<types::Boolean>& roundtrip_flights,
    const types::Optional<types::Date>& departure_or_return_date,
    const types::Optional<types::Boolean>& all_combinations) {
  QueryClass query(db_index);  // <- table processed by search class

  query.origin(origin);
  query.destinations(destinations);
  query.destination_countries(destination_countries);
  query.departure_dates(departure_date_from, departure_date_to);
  query.return_dates(return_date_from, return_date_to);
  query.departure_weekdays(departure_days_of_week);
  query.return_weekdays(return_days_of_week);
  query.stay_days(stay_from, stay_to);
  query.direct_flights(direct_flights);
  query.roundtrip_flights(roundtrip_flights);
  query.max_lifetime_sec(max_lifetime_sec);
  query.result_limit(limit);
  query.exact_departure_or_return_date(departure_or_return_date);
  query.calc_departue_return_max_duration(departure_date_from, departure_date_to, return_date_from,
                                          return_date_to);
  query.all_combinations(all_combinations);
  // load deals data from data pages (DealData shared memory pagers)
  return fill_deals_with_data(query.execute());
}
}  // namespace deals
#endif