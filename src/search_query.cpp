#include "search_query.hpp"

namespace query {
void SearchQuery::origin(std::string origin) {
  if (origin.length() == 0) {
    return;
  }
  filter_origin = true;
  origin_value = origin_to_code(origin);
}

void SearchQuery::destinations(std::string destinations) {
  if (destinations.length() == 0) {
    return;
  }
  std::vector<std::string> split_result = ::utils::split_string(destinations);

  for (auto& dst : split_result) {
    if (dst.length() == 3) {
      destination_values_vector.push_back(origin_to_code(dst));
    }
  }

  if (destination_values_vector.size()) {
    filter_destination = true;
  }
}

void SearchQuery::max_lifetime_sec(uint32_t max_lifetime) {
  if (max_lifetime == 0) {
    return;
  }

  filter_timestamp = true;
  timestamp_value = timing::getTimestampSec() - max_lifetime;
}

void SearchQuery::departure_dates(std::string departure_date_from, std::string departure_date_to) {
  if (departure_date_from.length() == 0 && departure_date_to.length() == 0) {
    return;
  }

  departure_date_values.duration = 0;
  if (departure_date_from.length() > 0 && departure_date_to.length() > 0) {
    departure_date_values.duration =
        utils::days_between_dates(departure_date_from, departure_date_to) + 1;
  }

  filter_departure_date = true;

  if (departure_date_from.length() == 0) {
    departure_date_values.from = 0;
  } else {
    departure_date_values.from = date_to_int(departure_date_from);
  }

  if (departure_date_to.length() == 0) {
    departure_date_values.to = UINT32_MAX;
  } else {
    departure_date_values.to = date_to_int(departure_date_to);
  }

  if (departure_date_values.from > departure_date_values.to) {
    query_is_broken = true;
  }
}

// comma separated list of dates to look up
void SearchQuery::departure_dates(std::string departure_dates) {
  if (departure_dates.length() == 0) {
    return;
  }

  auto dates = ::utils::split_string(departure_dates);
  for (auto& date : dates) {
    uint32_t date_int = date_to_int(date);
    if (date_int) {
      departure_dates_vector.push_back(date_int);
    }
  }

  if (departure_dates_vector.size()) {
    filter_departure_dates = true;
  }
}

// comma separated list of dates to look up
void SearchQuery::return_dates(std::string return_dates) {
  if (return_dates.length() == 0) {
    return;
  }

  auto dates = ::utils::split_string(return_dates);
  for (auto& date : dates) {
    uint32_t date_int = date_to_int(date);
    if (date_int) {
      return_dates_vector.push_back(date_int);
    }
  }

  if (return_dates_vector.size()) {
    filter_return_dates = true;
  }
}

void SearchQuery::return_dates(std::string return_date_from, std::string return_date_to) {
  if (return_date_from.length() == 0 && return_date_to.length() == 0) {
    return;
  }

  return_date_values.duration = 0;
  if (return_date_from.length() > 0 && return_date_to.length() > 0) {
    return_date_values.duration = utils::days_between_dates(return_date_from, return_date_to);
  }

  filter_return_date = true;

  if (return_date_from.length() == 0) {
    return_date_values.from = 0;
  } else {
    return_date_values.from = date_to_int(return_date_from);
  }

  if (return_date_to.length() == 0) {
    return_date_values.to = UINT32_MAX;
  } else {
    return_date_values.to = date_to_int(return_date_to);
  }

  if (return_date_values.from > return_date_values.to) {
    query_is_broken = true;
  }
  if (return_date_values.from == 0 && return_date_values.to == 0) {
    query_is_broken = true;
  }
}

void SearchQuery::stay_days(uint16_t stay_from, uint16_t stay_to) {
  if (stay_from == 0 && (stay_to == 0 || stay_to >= UINT8_MAX)) {
    return;
  }

  filter_stay_days = true;

  if (stay_from == 0) {
    stay_days_values.from = 0;
  } else {
    stay_days_values.from = stay_from;
  }

  if (stay_to == 0 || stay_to >= UINT8_MAX) {
    stay_days_values.to = UINT8_MAX;
  } else {
    stay_days_values.to = stay_to;
  }

  if (stay_days_values.from > stay_days_values.to) {
    query_is_broken = true;
  }
  if (stay_days_values.from == 0 && stay_days_values.to == 0) {
    query_is_broken = true;
  }
}

void SearchQuery::departure_weekdays(std::string days_of_week) {
  if (days_of_week.length() == 0) {
    return;
  }
  departure_weekdays_bitmask = weekdays_bitmask(days_of_week);
  filter_departure_weekdays = true;
}

void SearchQuery::return_weekdays(std::string days_of_week) {
  if (days_of_week.length() == 0) {
    return;
  }
  return_weekdays_bitmask = weekdays_bitmask(days_of_week);
  filter_return_weekdays = true;
}

uint8_t SearchQuery::weekdays_bitmask(std::string days_of_week) {
  uint8_t result = 0;
  std::vector<std::string> split_result = ::utils::split_string(days_of_week);

  std::vector<std::string>::iterator day = split_result.begin();
  for (; day != split_result.end(); ++day) {
    uint8_t daycode = ::utils::day_of_week_from_str(*day);
    if (daycode < 0 || daycode > 6) {
      std::cout << "bad daycode for " << *day << "-" << days_of_week << std::endl;
      continue;
    }
    result |= (1 << daycode);
  }

  if (result == 0) {
    std::cout << "bad bitmask for " << days_of_week << std::endl;
    query_is_broken = true;
  }

  return result;
}

void SearchQuery::direct_flights(bool direct_flights, bool stops_flights) {
  if (direct_flights == true && stops_flights == true) {
    return;
  }

  filter_flight_by_stops = true;
  direct_flights_flag = direct_flights;
  stops_flights_flag = stops_flights;

  if (direct_flights_flag == false && stops_flights_flag == false) {
    query_is_broken = true;
  }
}

void SearchQuery::result_limit(uint16_t _filter_limit) {
  if (_filter_limit) {
    // ignore zero value
    filter_limit = _filter_limit;
  }
}

void SearchQuery::price(uint32_t price_from, uint32_t price_to) {
  if (price_from == 0 && price_to == 0) {
    // no filter applied
    return;
  }

  filter_price = true;
  price_from_value = price_from;
  price_to_value = price_to ? price_to : UINT32_MAX;
}

void SearchQuery::locale(std::string locale) {
  if (locale.length() != 2) {
    return;
  }

  filter_locale = true;
  locale_value = locale_to_code(locale);
}

void SearchQuery::apply_filters(std::string _origin, std::string _destinations,
                                std::string _departure_date_from, std::string _departure_date_to,
                                std::string _departure_days_of_week, std::string _return_date_from,
                                std::string _return_date_to, std::string _return_days_of_week,
                                uint16_t _stay_from, uint16_t _stay_to, bool _direct_flights,
                                bool _stops_flights, uint32_t _price_from, uint32_t _price_to,
                                uint16_t _limit, uint32_t _max_lifetime_sec) {
  origin(_origin);
  destinations(_destinations);
  departure_dates(_departure_date_from, _departure_date_to);
  return_dates(_return_date_from, _return_date_to);
  stay_days(_stay_from, _stay_to);
  direct_flights(_direct_flights, _stops_flights);
  result_limit(_limit);
  max_lifetime_sec(_max_lifetime_sec);
  departure_weekdays(_departure_days_of_week);
  return_weekdays(_return_days_of_week);
  price(_price_from, _price_to);
}

//--------------------------------------------------
// check_destinations_format
//--------------------------------------------------
bool check_destinations_format(std::string destinations) {
  if (destinations.length() == 0) {
    return true;
  }

  std::vector<std::string> split_result = ::utils::split_string(destinations);

  for (auto& dst : split_result) {
    if (dst.length() != 3) {
      return false;
    }
  }

  return true;
}

//--------------------------------------------------
// check_weekdays_format
//--------------------------------------------------
bool check_weekdays_format(std::string weekdays) {
  if (weekdays.length() == 0) {
    return true;
  }

  std::vector<std::string> split_result = ::utils::split_string(weekdays);

  for (auto& day : split_result) {
    if (day.length() != 3) {
      return false;
    }

    if (::utils::day_of_week_from_str(day) > 6) {
      return false;
    }
  }

  return true;
}

//--------------------------------------------------
// check_date_format
//--------------------------------------------------
bool check_date_format(std::string date) {
  return date_to_int(date) != 0;
}

//--------------------------------------------------
// check_date_to_date
//--------------------------------------------------
bool check_date_to_date(std::string _date_from, std::string _date_to) {
  uint32_t date_from = date_to_int(_date_from);
  uint32_t date_to = date_to_int(_date_to);
  if (date_from && date_to) {
    return date_from <= date_to;
  }
  return true;
}

//--------------------------------------------------
// deals Utils
//--------------------------------------------------
uint32_t origin_to_code(std::string code) {
  PlaceCodec a2i;
  a2i.iata_code[0] = 0;
  a2i.iata_code[1] = code[0];
  a2i.iata_code[2] = code[1];
  a2i.iata_code[3] = code[2];
  return a2i.int_code;
}

//--------------------------------------------------
//
//--------------------------------------------------
std::string code_to_origin(uint32_t code) {
  PlaceCodec a2i;
  a2i.int_code = code;
  std::string result;
  result += a2i.iata_code[1];
  result += a2i.iata_code[2];
  result += a2i.iata_code[3];
  return result;
}

//--------------------------------------------------
// ISO date standare 2016-06-16
//--------------------------------------------------
uint32_t date_to_int(std::string date) {
  if (date.length() != 10) {
    return 0;
  }
  if (date[4] != '-' || date[7] != '-') {
    return 0;
  }

  date.erase(4, 1);
  date.erase(6, 1);

  try {
    return std::stol(date);
  } catch (std::exception e) {
    return 0;
  }
};

//--------------------------------------------------
// ISO date standare 2016-06-16
//--------------------------------------------------
std::string int_to_date(uint32_t date) {
  if (!date) {
    return "";
  }

  std::string result;
  uint16_t year;
  uint16_t month;
  uint16_t day;

  // 20160601
  year = date / 10000;
  month = (date - year * 10000) / 100;
  day = date - year * 10000 - month * 100;

  result = std::to_string(year) + "-" + (month < 10 ? "0" : "") + std::to_string(month) + "-" +
           (day < 10 ? "0" : "") + std::to_string(day);
  return result;
};

//-------------------------------------------
// locale_to_code
//-------------------------------------------
uint16_t locale_to_code(std::string locale) {
  LocaleCodec a2i;
  a2i.text_code[0] = locale[0];
  a2i.text_code[1] = locale[1];
  return a2i.int_code;
}

//-------------------------------------------
// code_to_locale
//-------------------------------------------
std::string code_to_locale(uint32_t code) {
  LocaleCodec a2i;
  a2i.int_code = code;
  std::string result;
  result += a2i.text_code[0];
  result += a2i.text_code[1];
  return result;
}

}  // deals namespace
