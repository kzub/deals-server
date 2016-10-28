#ifndef SRC_SEARCH_QUERY_HPP
#define SRC_SEARCH_QUERY_HPP

#include <unordered_set>
#include "shared_memory.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace query {
struct DateInterval {
  uint32_t from;
  uint32_t to;
  uint32_t duration;
};

struct StayInterval {
  uint8_t from;
  uint8_t to;
};

class SearchQuery {
 public:
  void origin(const types::IATACode& origin);
  void destinations(const types::IATACodes& destinations);
  void destination_countries(const types::CountryCodes& countries);
  void departure_dates(const types::Date& departure_date_from,
                       const types::Date& departure_date_to);
  void return_dates(const types::Date& return_date_from,  //
                    const types::Date& return_date_to);
  void direct_flights(const types::Boolean& direct_flights);
  void roundtrip_flights(const types::Boolean& roundtrip);
  void max_lifetime_sec(const types::Number& max_lifetime);
  void result_limit(const types::Number& limit);
  void stay_days(const types::Number& stay_from,  //
                 const types::Number& stay_to);
  void departure_weekdays(const types::Weekdays& days_of_week);
  void return_weekdays(const types::Weekdays& days_of_week);
  void locale(const types::CountryCode& locale);

 protected:
  bool filter_origin = false;
  uint32_t origin_value;

  bool filter_destination = false;
  std::unordered_set<uint32_t> destination_values_set;

  bool filter_destination_country = false;
  std::unordered_set<uint8_t> destination_country_set;

  bool filter_departure_date = false;
  DateInterval departure_date_values;

  bool filter_return_date = false;
  DateInterval return_date_values;

  bool filter_timestamp = false;
  uint32_t timestamp_value;

  bool filter_flight_by_stops = false;
  bool direct_flights_flag;

  bool filter_flight_by_roundtrip = false;
  bool roundtrip_flight_flag;

  bool filter_departure_weekdays = false;
  uint8_t departure_weekdays_bitmask;

  bool filter_return_weekdays = false;
  uint8_t return_weekdays_bitmask;

  bool filter_stay_days = false;
  StayInterval stay_days_values;

  uint16_t filter_result_limit = 20;

  bool filter_locale = false;
  uint8_t locale_value;
};

}  // namespace query

#endif