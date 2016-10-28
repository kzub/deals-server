#include "search_query.hpp"

namespace query {

//--------------------------------------------------
void SearchQuery::origin(const types::IATACode& origin) {
  if (origin.isUndefined()) {
    return;
  }

  filter_origin = true;
  origin_value = origin.get_code();
}

//--------------------------------------------------
void SearchQuery::destinations(const types::IATACodes& destinations) {
  if (destinations.isUndefined()) {
    return;
  }
  destination_values_set.clear();

  auto it = destinations.get_codes();
  for (const auto& dst : it) {
    destination_values_set.insert(dst.get_code());
  }

  if (destination_values_set.size()) {
    filter_destination = true;
  }
}

//--------------------------------------------------
void SearchQuery::destination_countries(const types::CountryCodes& countries) {
  if (countries.isUndefined()) {
    return;
  }
  destination_country_set.clear();

  auto it = countries.get_codes();
  for (const auto& country : it) {
    destination_country_set.insert(country.get_code());
  }

  if (destination_country_set.size()) {
    filter_destination_country = true;
  }
}

//--------------------------------------------------
void SearchQuery::max_lifetime_sec(const types::Number& max_lifetime) {
  if (max_lifetime.isUndefined()) {
    return;
  }

  filter_timestamp = true;
  timestamp_value = timing::getTimestampSec() - max_lifetime.get_value();
}

//--------------------------------------------------
void SearchQuery::departure_dates(const types::Date& departure_date_from,
                                  const types::Date& departure_date_to) {
  if (departure_date_from.isUndefined() && departure_date_to.isUndefined()) {
    return;
  }
  filter_departure_date = true;

  if (departure_date_from.isDefined() && departure_date_to.isDefined()) {
    departure_date_values.duration = departure_date_to.days_after(departure_date_from) + 1;
  } else {
    departure_date_values.duration = 0;
  }

  if (departure_date_from.isDefined()) {
    departure_date_values.from = departure_date_from.get_code();
  } else {
    departure_date_values.from = 0;
  }

  if (departure_date_to.isDefined()) {
    departure_date_values.to = departure_date_to.get_code();
  } else {
    departure_date_values.to = UINT32_MAX;
  }

  if (departure_date_values.from > departure_date_values.to) {
    throw types::Error("departure_dates departure_date_from > departure_date_to\n");
  }
}

//--------------------------------------------------
void SearchQuery::return_dates(const types::Date& return_date_from,
                               const types::Date& return_date_to) {
  if (return_date_from.isUndefined() && return_date_to.isUndefined()) {
    return;
  }
  filter_return_date = true;

  if (return_date_from.isDefined() && return_date_to.isDefined()) {
    return_date_values.duration = return_date_to.days_after(return_date_from) + 1;
  } else {
    return_date_values.duration = 0;
  }

  if (return_date_from.isDefined()) {
    return_date_values.from = return_date_from.get_code();
  } else {
    return_date_values.from = 0;
  }

  if (return_date_to.isDefined()) {
    return_date_values.to = return_date_to.get_code();
  } else {
    return_date_values.to = UINT32_MAX;
  }

  if (return_date_values.from > return_date_values.to) {
    throw types::Error("departure_dates return_date_from > return_date_to\n");
  }
}

//--------------------------------------------------
void SearchQuery::stay_days(const types::Number& stay_from, const types::Number& stay_to) {
  if (stay_from.isUndefined() && stay_to.isUndefined()) {
    return;
  }
  filter_stay_days = true;

  if (stay_from.isDefined() && stay_from.get_value() > UINT8_MAX) {
    throw "stay_from > 255";
  }

  if (stay_to.isDefined() && stay_to.get_value() > UINT8_MAX) {
    throw "stay_to > 255";
  }

  stay_days_values.from = stay_from.isDefined() ? stay_from.get_value() : 0;
  stay_days_values.to = stay_to.isDefined() ? stay_to.get_value() : UINT8_MAX;

  if (stay_days_values.from > stay_days_values.to) {
    throw "stay_from > stay_to";
  }
}

//--------------------------------------------------
void SearchQuery::departure_weekdays(const types::Weekdays& days_of_week) {
  if (days_of_week.isUndefined()) {
    return;
  }
  filter_departure_weekdays = true;
  departure_weekdays_bitmask = days_of_week.get_bitmask();
}

//--------------------------------------------------
void SearchQuery::return_weekdays(const types::Weekdays& days_of_week) {
  if (days_of_week.isUndefined()) {
    return;
  }
  filter_return_weekdays = true;
  return_weekdays_bitmask = days_of_week.get_bitmask();
}

//--------------------------------------------------
void SearchQuery::direct_flights(const types::Boolean& direct_flights) {
  if (direct_flights.isUndefined()) {
    return;
  }
  filter_flight_by_stops = true;
  direct_flights_flag = direct_flights.isTrue();
}

//--------------------------------------------------
void SearchQuery::roundtrip_flights(const types::Boolean& roundtrip) {
  if (roundtrip.isUndefined()) {
    return;
  }
  filter_flight_by_roundtrip = true;
  roundtrip_flight_flag = roundtrip.isTrue();
}

//--------------------------------------------------
void SearchQuery::result_limit(const types::Number& limit) {
  if (limit.isUndefined()) {
    return;
  }
  filter_result_limit = limit.get_value();
}

//--------------------------------------------------
void SearchQuery::locale(const types::CountryCode& locale) {
  if (locale.isUndefined()) {
    return;
  }
  filter_locale = true;
  locale_value = locale.get_code();
}

}  // deals namespace
