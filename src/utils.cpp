#include <algorithm>
#include <cinttypes>

#include "utils.hpp"

namespace utils {
//------------------------------------------------------------------
//  split_string       by delimiter and put it into vector
//------------------------------------------------------------------
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

//------------------------------------------------------------------
// concat_string
//------------------------------------------------------------------
std::string concat_string(const std::vector<std::string> msgs) {
  std::string concated_msg;
  for (auto& msg : msgs) {
    // std::cout << msg << " " << msg.size() << std::endl;
    concated_msg += msg;
  }
  return concated_msg;
}

//------------------------------------------------------------------
// toLowerCase
//------------------------------------------------------------------
std::string toLowerCase(std::string text) {
  // transform lowercase
  std::transform(text.begin(), text.end(), text.begin(), ::tolower);
  return text;
}

//------------------------------------------------------------------
// toUpperCase
//------------------------------------------------------------------
std::string toUpperCase(std::string text) {
  // transform lowercase
  std::transform(text.begin(), text.end(), text.begin(), ::toupper);
  return text;
}

//-----------------------------------------------------
// day_of_week
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
// day_of_week_from_date
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
// day_of_week_from_str
//-----------------------------------------------------------
uint8_t day_of_week_from_str(const std::string _weekday) {
  std::string weekday = toLowerCase(_weekday);
  for (uint8_t i = 0; i <= 6; ++i) {
    if (days[i] == weekday) {
      return i;
    }
  }
  return 7;  // error
}

uint8_t day_of_week_bitmask_from_str(const std::string _weekday) {
  return 1 << day_of_week_from_str(_weekday);
}

//-----------------------------------------------------------
// day_of_week_str_from_code
//-----------------------------------------------------------
std::string day_of_week_str_from_bitmask(const uint8_t code) {
  switch (code) {
    case 0b00000001:
      return days[0];
    case 0b00000010:
      return days[1];
    case 0b00000100:
      return days[2];
    case 0b00001000:
      return days[3];
    case 0b00010000:
      return days[4];
    case 0b00100000:
      return days[5];
    case 0b01000000:
      return days[6];
    default:
      return days[7];
  }
}

//-----------------------------------------------------------
// day_of_week_str_from_date
//-----------------------------------------------------------
std::string day_of_week_str_from_date(const std::string date) {
  uint8_t day = day_of_week_from_date(date);
  return days[day];
}

//-----------------------------------------------------------
// rdn
//-----------------------------------------------------------
uint32_t rdn(int y, int m, int d) { /* Rata Die day one is 0001-01-01 */
  if (m < 3) {
    y--;
    m += 12;
  }
  return 365 * y + y / 4 - y / 100 + y / 400 + (153 * m - 457) / 5 + d - 306;
}

//-----------------------------------------------------------
// days_between_dates
//-----------------------------------------------------------
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

uint32_t days_between_dates(const uint16_t date1_year, const uint8_t date1_month,
                            const uint8_t date1_day, const uint16_t date2_year,
                            const uint8_t date2_month, const uint8_t date2_day) {
  const uint32_t date1_days = rdn(date1_year, date1_month, date1_day);
  const uint32_t date2_days = rdn(date2_year, date2_month, date2_day);
  return date2_days - date1_days;
}
}  // namespace utils