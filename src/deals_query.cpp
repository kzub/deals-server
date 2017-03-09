#include "deals_query.hpp"

namespace deals {
// ----------------------------------------------------------
std::vector<i::DealInfo> DealsSearchQuery::execute() {
  current_time = timing::getTimestampSec();

  pre_search();  // run in derived class

  // table processor iterates table pages and call DealsSearchQuery::process_element()
  table.processRecords(*this);

  post_search();  // run in derived class

  auto result = get_result();

  // reduce output size
  if (result.size() > filter_result_limit) {
    result.resize(filter_result_limit);
  }

  return result;
};

//----------------------------------------------------------------
// DealsSearchQuery process_element()
// function that will be called by TableProcessor
// for iterating over all not expired pages in table
void DealsSearchQuery::process_element(const i::DealInfo &deal) {
  // check if not expired
  if ((deal.timestamp + DEALS_EXPIRES < current_time) ||
      (deal.timestamp + DEALS_EXPIRES < shared_mem::global_expire_at)) {  // =( rethink it later
    return;
  }

  if (filter_origin && origin_value != deal.origin) {
    return;
  }

  if (filter_timestamp && timestamp_value > deal.timestamp) {
    return;
  }

  if (filter_flight_by_roundtrip) {
    if (roundtrip_flight_flag == true) {
      if (deal.return_date == 0) {
        return;
      }
    } else {
      if (deal.return_date != 0) {
        return;
      }
    }
  }

  if (filter_destination) {
    if (destination_values_set.find(deal.destination) == destination_values_set.end()) {
      return;
    }
  }

  if (filter_destination_country) {
    if (destination_country_set.find(deal.destination_country) == destination_country_set.end()) {
      return;
    }
  }

  if (filter_departure_date && (deal.departure_date < departure_date_values.from ||
                                deal.departure_date > departure_date_values.to)) {
    return;
  }

  if (filter_return_date &&
      (deal.return_date < return_date_values.from || deal.return_date > return_date_values.to)) {
    return;
  }

  if (filter_stay_days &&
      (deal.stay_days < stay_days_values.from || deal.stay_days > stay_days_values.to)) {
    return;
  }

  if (filter_flight_by_stops && (direct_flights_flag != deal.direct)) {
    return;
  }

  if (filter_departure_weekdays && (deal.departure_day_of_week & departure_weekdays_bitmask) == 0) {
    return;
  }

  if (filter_return_weekdays && (deal.return_day_of_week & return_weekdays_bitmask) == 0) {
    return;
  }

  // Deal matched all selected filters -> process it @ derivered class
  process_deal(deal);
}
}  // namespace deals