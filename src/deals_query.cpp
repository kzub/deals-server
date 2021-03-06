#include "deals_query.hpp"

namespace deals {
// ----------------------------------------------------------
std::vector<i::DealInfo> DealsSearchQuery::execute() {
  pre_search();  // run in derived class

  // filter out expired deals inside record, record has many elements with different ages,
  // but record age equal max age of all elements inside it.  =/ rethink it later
  min_timestamp =
      std::max(table.context.shm.global_expire_at, timing::getTimestampSec()) - DEALS_EXPIRES;

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
  // check if not expired by now or data page was reused on lowMem (global_expire_at)
  if (deal.timestamp <= min_timestamp) {
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

  if (filter_exact_date &&  //
      deal.return_date != exact_date_value && deal.departure_date != exact_date_value) {
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