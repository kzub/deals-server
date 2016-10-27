#include <iostream>

//--------------------------------------------------
// date_to_int          ISO date standare 2016-06-16
//--------------------------------------------------
uint32_t date_to_int(std::string date) {
  if (date.length() == 0) {
    return 0;  // no date setted
  }

  if (date.length() != 10) {
    throw std::string("Date has wrong format:'" + date + "'\n");
  }

  if (date[4] != '-' || date[7] != '-') {
    throw std::string("Date has wrong format:'" + date + "'\n");
  }

  date.erase(4, 1);
  date.erase(6, 1);

  try {
    return std::stol(date);
  } catch (std::exception e) {
    throw std::string("Date has wrong format:'" + date + "'\n");
  }
};

class Date {
 public:
  Date(std::string date);
  const uint32_t code;
};

class Weekdays {
 public:
  Weekdays(std::string wd);
  Weekdays(Date wd);
  const uint8_t bitmask;
};
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

//------------------------------------------------------------------------
// Date
//------------------------------------------------------------------------
Date::Date(std::string _date) : code(date_to_int(_date)) {
}

uint8_t day_of_week_from_date(const Date date) {
  uint16_t year = date.code / 10000;
  uint8_t month = (date.code / 100) % 100;
  uint8_t day = date.code % 100;

  std::cout << ">>" << date.code << std::endl;
  std::cout << "year:" << std::to_string(year) << std::endl;
  std::cout << "month:" << std::to_string(month) << std::endl;
  std::cout << "day:" << std::to_string(day) << std::endl;
  return date.code;
}

//------------------------------------------------------------------------
// Weekdays
//------------------------------------------------------------------------
Weekdays::Weekdays(Date wd) : bitmask(day_of_week_from_date(wd)) {
}

int main() {
  try {
    Date date("2016-02-01");
    Weekdays w(date);
    std::cout << "OK:" << std::to_string(w.bitmask) << std::endl;
  } catch (...) {
    std::cout << "SsSS" << std::endl;
  }
  return 0;
}