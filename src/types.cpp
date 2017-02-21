#include "types.hpp"
#include "utils.hpp"

namespace types {
//------------------------------------------------------------------
// ObjectMap [] accessor
//------------------------------------------------------------------
std::string ObjectMap::operator[](const std::string name) {
  for (const auto& obj : mapStorage) {
    if (obj.name == name) {
      return obj.value;
    }
  }
  return {};
}

//------------------------------------------------------------------
// ObjectMap add_object
//------------------------------------------------------------------
void ObjectMap::add_object(const Object obj) {
  mapStorage.push_back(obj);
}

//------------------------------------------------------------------------
// IATACode
//------------------------------------------------------------------------
IATACode::IATACode(std::string iatacode) {
  if (iatacode.length() == 0) {
    paramter_undefined = true;
    return;
  }

  if (iatacode.length() != 3) {
    throw Error("Bad IATA code:" + iatacode + "\n");
  }
  code = origin_to_code(utils::toUpperCase(iatacode));
}

IATACode::IATACode(uint32_t dbcode) {
  if (dbcode == 0) {
    throw Error("Bad IATA code:" + std::to_string(dbcode) + "\n", ErrorCode::InternalError);
  }

  code = dbcode;
}

bool operator==(const IATACode& d1, const IATACode& d2) {
  if (d1.isUndefined() || d2.isUndefined()) {
    return false;
  }

  return d1.code == d2.code;
}

uint32_t IATACode::get_code() const {
  if (paramter_undefined) {
    throw Error("IATACode parameter not defined\n");
  }
  return code;
}

//------------------------------------------------------------------------
// IATACodes
//------------------------------------------------------------------------
IATACodes::IATACodes(std::string _codes) {
  if (_codes.length() == 0) {
    paramter_undefined = true;
    return;
  }

  const auto split_result = utils::split_string(_codes);
  for (const auto& code : split_result) {
    codes.insert({code});
  }
}

void IATACodes::add_code(uint32_t dbcode) {
  codes.insert({dbcode});
  paramter_undefined = false;
}

const std::unordered_set<IATACode, CodeHash>& IATACodes::get_codes() const {
  return codes;
}

//------------------------------------------------------------------------
// CountryCode
//------------------------------------------------------------------------
CountryCode::CountryCode(std::string _code) {
  if (_code.length() == 0) {
    paramter_undefined = true;
    return;
  }

  if (_code.length() != 2) {
    throw Error("CountryCode format error:" + _code + "\n");
  }

  code = country_to_code(utils::toUpperCase(_code));
}

CountryCode::CountryCode(uint8_t dbcode) {
  if (dbcode >= COUNTRIES.size()) {
    throw Error("Bad Country code:" + std::to_string(dbcode) + "\n", ErrorCode::InternalError);
  }

  code = dbcode;
}

bool operator==(const CountryCode& d1, const CountryCode& d2) {
  if (d1.isUndefined() || d2.isUndefined()) {
    return false;
  }

  return d1.code == d2.code;
}

uint8_t CountryCode::get_code() const {
  if (paramter_undefined) {
    throw Error("CountryCode parameter not defined\n");
  }
  return code;
}

//------------------------------------------------------------------------
// CountryCodes
//------------------------------------------------------------------------
CountryCodes::CountryCodes(std::string _codes) {
  if (_codes.length() == 0) {
    paramter_undefined = true;
    return;
  }

  const auto split_result = utils::split_string(_codes);
  for (const auto& code : split_result) {
    codes.insert({code});
  }
}

void CountryCodes::add_code(uint8_t dbcode) {
  codes.insert({dbcode});
  paramter_undefined = false;
}

const std::unordered_set<CountryCode, CodeHash>& CountryCodes::get_codes() const {
  return codes;
}

//------------------------------------------------------------------------
// Date
//------------------------------------------------------------------------
Date::Date(std::string _date) {
  if (_date.length() == 0) {
    paramter_undefined = true;
    return;
  }

  code = date_to_int(_date);
  day = code % 100;
  month = (code / 100) % 100;
  year = code / 10000;
}

uint32_t Date::get_code() const {
  if (paramter_undefined) {
    throw Error("Date parameter not defined\n");
  }
  return code;
}
uint16_t Date::get_year() const {
  if (paramter_undefined) {
    throw Error("Date parameter not defined\n");
  }
  return year;
}
uint8_t Date::get_month() const {
  if (paramter_undefined) {
    throw Error("Date parameter not defined\n");
  }
  return month;
}
uint8_t Date::get_day() const {
  if (paramter_undefined) {
    throw Error("Date parameter not defined\n");
  }
  return day;
}

uint32_t Date::days_after(Date const date) const {
  return utils::days_between_dates(date.year, date.month, date.day, this->year, this->month,
                                   this->day);
}

uint8_t Date::day_of_week() const {
  return utils::day_of_week(day, month, year);
}

bool operator<(const Date& d1, const Date& d2) {
  if (d1.isUndefined() || d2.isUndefined()) {
    return false;
  }
  return d1.code < d2.code;
}
bool operator<=(const Date& d1, const Date& d2) {
  if (d1.isUndefined() || d2.isUndefined()) {
    return false;
  }
  return d1.code <= d2.code;
}
bool operator>(const Date& d1, const Date& d2) {
  if (d1.isUndefined() || d2.isUndefined()) {
    return false;
  }
  return d1.code > d2.code;
}
bool operator>=(const Date& d1, const Date& d2) {
  if (d1.isUndefined() || d2.isUndefined()) {
    return false;
  }
  return d1.code >= d2.code;
}
bool operator==(const Date& d1, const Date& d2) {
  if (d1.isUndefined() || d2.isUndefined()) {
    return false;
  }
  return d1.code == d2.code;
}
//------------------------------------------------------------------------
// Weekdays
//------------------------------------------------------------------------
Weekdays::Weekdays(std::string wd) {
  if (wd.length() == 0) {
    paramter_undefined = true;
    return;
  }

  bitmask = weekdays_bitmask(wd);
}

Weekdays::Weekdays(Date date) {
  bitmask = 1 << date.day_of_week();
}

uint8_t Weekdays::get_bitmask() const {
  if (paramter_undefined) {
    throw Error("Weekdays value not defined\n");
  }
  return bitmask;
}

//------------------------------------------------------------------------
// Number
//------------------------------------------------------------------------
Number::Number(std::string number) {
  if (number.length() == 0) {
    paramter_undefined = true;
    return;
  }

  value = std::stol(number);
}

Number::Number(uint32_t number) {
  value = number;
}

uint32_t Number::get_value() const {
  if (paramter_undefined) {
    throw Error("Number value not defined\n");
  }
  return value;
}

//------------------------------------------------------------------------
// Boolean
//------------------------------------------------------------------------
Boolean::Boolean(std::string param) {
  if (param.length() == 0) {
    paramter_undefined = true;
    return;
  }

  const std::string p = utils::toLowerCase(param);
  if (p == "true") {
    value = true;
  } else if (p == "false") {
    value = false;
  } else {
    throw Error("Not a boolean type:" + param + "\n");
  }
}

Boolean::Boolean(bool val) {
  value = val;
}

bool Boolean::isTrue() const {
  if (paramter_undefined) {
    throw Error("Boolean value not defined\n");
  }
  return value;
}
bool Boolean::isFalse() const {
  if (paramter_undefined) {
    throw Error("Boolean value not defined\n");
  }
  return !value;
}

//--------------------------------------------------
// weekdays_bitmask
//--------------------------------------------------
uint8_t weekdays_bitmask(std::string days_of_week) {
  uint8_t result = 0;
  if (days_of_week.length() == 0) {
    throw Error("Day of week undefined\n");
  }

  const auto split_result = utils::split_string(days_of_week);

  for (const auto& day : split_result) {
    uint8_t daymask = utils::day_of_week_bitmask_from_str(day);
    if (daymask > 64) {
      throw Error("Bad day '" + day + "' in [" + days_of_week + "]\n");
    }
    result |= daymask;
  }

  if (result == 0) {
    throw Error("Cannot parse days of week:" + days_of_week + "\n");
  }

  return result;
}

//--------------------------------------------------
// origin_to_code
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
// code_to_origin
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
// date_to_int          ISO date standare 2016-06-16
//--------------------------------------------------
uint32_t date_to_int(std::string date) {
  if (date.length() != 10) {
    throw Error("wrong date format:'" + date + "'\n");
  }

  if (date[4] != '-' || date[7] != '-') {
    throw Error("wrong date format:'" + date + "'\n");
  }

  date.erase(4, 1);
  date.erase(6, 1);

  try {
    return std::stol(date);
  } catch (...) {
    throw Error("wrong date format:'" + date + "'\n");
  }
};

//--------------------------------------------------
// int_to_date          ISO date standare 2016-06-16
//--------------------------------------------------
std::string int_to_date(uint32_t date) {
  if (date == 0) {
    throw Error("wrong date code:'" + std::to_string(date) + "'\n", ErrorCode::InternalError);
  }

  // 20160601
  uint16_t year = date / 10000;
  uint16_t month = (date - year * 10000) / 100;
  uint16_t day = date - year * 10000 - month * 100;

  std::string result = std::to_string(year) + "-" + (month < 10 ? "0" : "") +
                       std::to_string(month) + "-" + (day < 10 ? "0" : "") + std::to_string(day);
  return result;
};

//-------------------------------------------
// country_to_code
//-------------------------------------------
uint8_t country_to_code(std::string country) {
  for (auto x = 0; x < COUNTRIES.size(); ++x) {
    const auto& code = COUNTRIES[x];
    if (code[0] == country[0] && code[1] == country[1]) {
      return x;
    }
  }
  throw Error("unknown country:" + country + "\n");
}

//-------------------------------------------
// code_to_country
//-------------------------------------------
std::string code_to_country(uint8_t code) {
  if (code > COUNTRIES.size()) {
    throw Error("CountryCode bad code:'" + std::to_string(code) + "'\n", ErrorCode::InternalError);
  }

  return COUNTRIES[code];
}
}