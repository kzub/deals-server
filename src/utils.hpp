#ifndef SRC_UTILS_HPP
#define SRC_UTILS_HPP

#include <iostream>
#include <vector>

namespace utils {
/*-----------------------------------------------------
  key value storage for internal use
-----------------------------------------------------*/
struct Object {
  std::string name;
  std::string value;
};

/*------------------------------------------------------------------
* Key-Value container and accessor
------------------------------------------------------------------*/
class ObjectMap {
 public:
  // params accessor
  std::string operator[](const std::string name);
  void add_object(const Object obj);

 private:
  std::vector<Object> mapStorage;
};

/*-----------------------------------------------------
  split strings by delimiter and put it into vector
-----------------------------------------------------*/
std::vector<std::string> split_string(std::string text, const std::string delimiter = ",");

/*------------------------------------------------------------------
* util: concat string
------------------------------------------------------------------*/
std::string concat_string(const std::vector<std::string> msgs);

/*-----------------------------------------------------
  utils: search by key in object storage
-----------------------------------------------------*/
std::string findValueInObjs(const std::vector<Object> objs, const std::string name);

/*-----------------------------------------------------
  utils: search by key in object storage
-----------------------------------------------------*/
std::string toLowerCase(std::string);
std::string toUpperCase(std::string);

/*-----------------------------------------------------
  utils: date related utils
-----------------------------------------------------*/
static std::string days[] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun", ""};

uint8_t day_of_week(uint8_t d, uint8_t m, uint16_t y);
uint8_t day_of_week_from_date(const std::string date);
uint8_t day_of_week_from_str(const std::string weekday);

uint32_t days_between_dates(const std::string date1, const std::string date2);
std::string day_of_week_str_from_date(const std::string date);
std::string day_of_week_str_from_code(const uint8_t);
/*-----------------------------------------------------
  utils: convert to int
-----------------------------------------------------*/
template <typename INT_T>
INT_T string_to_int(const std::string text);

// TEMPLATE IMPLEMENTATIONS
template <typename INT_T>
INT_T string_to_int(const std::string text) {
  // transform lowercase
  INT_T result = 0;
  try {
    result = std::stol(text);
  } catch (...) {
  }
  return result;
}
}
#endif