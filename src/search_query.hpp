#ifndef SRC_SEARCH_QUERY_HPP
#define SRC_SEARCH_QUERY_HPP

#include <set>
#include "shared_memory.hpp"
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
  void origin(std::string origin);
  void destinations(std::string destinations);
  void departure_dates(std::string departure_date_from, std::string departure_date_to);
  void departure_dates(std::string departure_dates);
  void return_dates(std::string return_date_from, std::string return_date_to);
  void return_dates(std::string return_dates);
  void direct_flights(utils::Threelean direct_flights);
  void max_lifetime_sec(uint32_t max_lifetime);
  void result_limit(uint16_t limit);
  void stay_days(uint16_t stay_from, uint16_t stay_to);
  void departure_weekdays(std::string days_of_week);
  void return_weekdays(std::string days_of_week);
  void price(uint32_t price_from, uint32_t price_to);
  void locale(std::string locale);
  void roundtrip_flights(utils::Threelean roundtrip);

  void apply_filters(std::string origin, std::string destinations, std::string departure_date_from,
                     std::string departure_date_to, std::string departure_days_of_week,
                     std::string return_date_from, std::string return_date_to,
                     std::string return_days_of_week, uint16_t stay_from, uint16_t stay_to,
                     utils::Threelean direct_flights, uint32_t price_from, uint32_t price_to,
                     uint16_t limit, uint32_t max_lifetime_sec, utils::Threelean roundtrip);

 protected:
  uint8_t weekdays_bitmask(std::string days_of_week);

  bool filter_origin = false;
  uint32_t origin_value;

  bool filter_destination = false;
  std::set<uint32_t> destination_values_set;

  bool filter_departure_date = false;
  DateInterval departure_date_values;

  // example: [2016-10-01, 2016-10-02, 2016-10-03]
  // NOT IMPLEMENTED YET:
  bool filter_departure_dates = false;
  std::vector<uint32_t> departure_dates_vector;

  bool filter_return_date = false;
  DateInterval return_date_values;

  // example: [2016-10-01, 2016-10-02, 2016-10-03]
  // NOT IMPLEMENTED YET:
  bool filter_return_dates = false;
  std::vector<uint32_t> return_dates_vector;

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

  uint16_t filter_limit = 20;

  bool filter_price = false;
  uint32_t price_from_value;
  uint32_t price_to_value;

  bool filter_locale = false;
  uint16_t locale_value;

  bool query_is_broken = false;
  std::string broken_query_error_msg;
};

bool check_destinations_format(std::string destinations);
bool check_weekdays_format(std::string weekdays);
bool check_date_format(std::string date);
bool check_date_to_date(std::string date_from, std::string date_to);

union PlaceCodec {
  uint32_t int_code;
  char iata_code[4];
};

uint32_t origin_to_code(std::string code);
std::string code_to_origin(uint32_t code);
uint32_t date_to_int(std::string date);
std::string int_to_date(uint32_t date);

union LocaleCodec {
  uint16_t int_code;
  char text_code[2];
};
uint16_t locale_to_code(std::string locale);
std::string code_to_locale(uint32_t code);

}  // namespace query

#endif