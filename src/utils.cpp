#include <algorithm>
#include <cinttypes>

#include "utils.hpp"

namespace utils {
/*------------------------------------------------------------------
* Params container and accessor
------------------------------------------------------------------*/
std::string ObjectMap::operator[](const std::string name) {
  return findValueInObjs(mapStorage, name);
}

void ObjectMap::add_object(const Object obj) {
  mapStorage.push_back(obj);
}

/*-----------------------------------------------------
  split strings by delimiter and put it into vector
-----------------------------------------------------*/
std::vector<std::string> split_string(std::string text, const std::string delimiter) {
  std::vector<std::string> result;

  while (text.length()) {
    size_t pos = text.find(delimiter);

    if (pos == -1) {
      result.push_back(text);
      return result;
    }

    std::string token = text.substr(0, pos);
    result.push_back(token);
    text = text.substr(pos + delimiter.length(), std::string::npos);
  }

  return result;
}

/*------------------------------------------------------------------
* util: concat string
------------------------------------------------------------------*/
std::string concat_string(const std::vector<std::string> msgs) {
  std::string concated_msg;
  for (auto msg : msgs) {
    // std::cout << msg << " " << msg.size() << std::endl;
    concated_msg += msg;
  }
  return concated_msg;
}

/*------------------------------------------------------------------
* util: lowercase
------------------------------------------------------------------*/
std::string toLowerCase(std::string text) {
  // transform lowercase
  std::transform(text.begin(), text.end(), text.begin(), ::tolower);
  return text;
}

/*------------------------------------------------------------------
* util: uppercase
------------------------------------------------------------------*/
std::string toUpperCase(std::string text) {
  // transform lowercase
  std::transform(text.begin(), text.end(), text.begin(), ::toupper);
  return text;
}

/*-----------------------------------------------------
  utils: search by key in object storage
-----------------------------------------------------*/
std::string findValueInObjs(const std::vector<Object> objs, const std::string name) {
  for (auto obj : objs) {
    if (obj.name == name) {
      return obj.value;
    }
  }
  std::string empty;
  return empty;
}

//-----------------------------------------------------
// utils: get week day by date
// http://www.geeksforgeeks.org/find-day-of-the-week-for-a-given-date/
//-----------------------------------------------------
uint8_t day_of_week(uint8_t d, uint8_t m, uint16_t y) {
  static uint8_t t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  y -= m < 3;
  uint8_t res = ((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7);
  // mon - 0
  // thu - 1
  // ...
  // sat - 5
  // sun - 6
  if (res == 0) {
    return 6;
  }
  return res - 1;
}

//-----------------------------------------------------------
//
//-----------------------------------------------------------
uint8_t day_of_week_from_date(const std::string date) {
  if (date[4] != '-' || date[7] != '-') {
    return 7;
  }
  std::string year = date.substr(0, 4);
  std::string month = date.substr(5, 7);
  std::string day = date.substr(8, 10);

  uint16_t int_year = 0;
  uint8_t int_month = 0;
  uint8_t int_day = 0;

  try {
    int_year = std::stoi(year);
    int_month = std::stoi(month);
    int_day = std::stoi(day);
  } catch (...) {
    return 7;
  }

  return day_of_week(int_day, int_month, int_year);
}

//-----------------------------------------------------------
//
//-----------------------------------------------------------
uint8_t day_of_week_from_str(const std::string _weekday) {
  std::string weekday = toLowerCase(_weekday);
  for (uint8_t i = 0; i <= 6; i++) {
    if (days[i] == weekday) {
      return i;
    }
  }
  return 7;  // error
}

//-----------------------------------------------------------
//
//-----------------------------------------------------------
std::string day_of_week_str_from_code(const uint8_t code) {
  if (code > 7) {
    return days[7];
  }
  return days[code];
}

//-----------------------------------------------------------
//
//-----------------------------------------------------------
std::string day_of_week_str_from_date(const std::string date) {
  uint8_t day = day_of_week_from_date(date);
  return days[day];
}

//-----------------------------------------------------------
//
//-----------------------------------------------------------
uint32_t rdn(int y, int m, int d) { /* Rata Die day one is 0001-01-01 */
  if (m < 3) {
    y--;
    m += 12;
  }
  return 365 * y + y / 4 - y / 100 + y / 400 + (153 * m - 457) / 5 + d - 306;
}

uint32_t days_between_dates(const std::string date1, const std::string date2) {
  if (date1[4] != '-' || date1[7] != '-' || date2[4] != '-' || date2[7] != '-') {
    return UINT16_MAX;
  }

  std::string year = date1.substr(0, 4);
  std::string month = date1.substr(5, 7);
  std::string day = date1.substr(8, 10);

  uint16_t int_year;
  uint8_t int_month;
  uint8_t int_day;

  try {
    int_year = std::stoi(year);
    int_month = std::stoi(month);
    int_day = std::stoi(day);
  } catch (...) {
    return UINT32_MAX;
  }

  uint32_t date1_days = rdn(int_year, int_month, int_day);

  year = date2.substr(0, 4);
  month = date2.substr(5, 7);
  day = date2.substr(8, 10);

  try {
    int_year = std::stoi(year);
    int_month = std::stoi(month);
    int_day = std::stoi(day);
  } catch (...) {
    return UINT32_MAX;
  }

  uint32_t date2_days = rdn(int_year, int_month, int_day);

  return date2_days - date1_days;
}
}