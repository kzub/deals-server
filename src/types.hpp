#ifndef SRC_TYPES_HPP
#define SRC_TYPES_HPP

#include <array>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace types {  // deals server types
//-----------------------------------------------------
//  key value storage for internal use
//-----------------------------------------------------
struct Object {
  std::string name;
  std::string value;
};

//------------------------------------------------------------------
// Key-Value container and accessor
//------------------------------------------------------------------
class ObjectMap {
 public:
  // params accessor
  std::string operator[](const std::string name);
  void add_object(const Object obj);

 private:
  std::vector<Object> mapStorage;
};

//------------------------------------------------------------
// Error codes
//------------------------------------------------------------
enum class ErrorCode : uint16_t { BadParameter, InternalError };

class Error {
 public:
  Error(std::string _text, ErrorCode _code = ErrorCode::BadParameter)
      : message(_text), code(_code) {
  }

  const std::string message;
  const ErrorCode code;
};

//------------------------------------------------------------
// Parameter Handlers
//------------------------------------------------------------
template <typename Base>
class Required;

// OPTIONAL parameter may not be defined
template <typename Base>
class Optional : public Base {
 public:
  Optional(ObjectMap params, std::string name) try : Base(params[name]) {
    parameter_name = name;
    parameter_value = params[name];
  } catch (Error err) {
    throw Error("Bad parameter:" + name + ". " + err.message, ErrorCode::BadParameter);
  };

 private:
  std::string parameter_name;
  std::string parameter_value;
  friend class Required<Base>;
};

// REQUIRED parameter must be defined
template <typename Base>
class Required : public Base {
 public:
  Required(ObjectMap params, std::string name) try : Base(params[name]) {
    if (this->isUndefined()) {
      throw Error("Must be defined\n", ErrorCode::BadParameter);
    }
  } catch (Error err) {
    throw Error("Bad parameter:" + name + ". " + err.message, ErrorCode::BadParameter);
  };

  Required(const Optional<Base>& parameter) try : Base(parameter.parameter_value) {
    if (this->isUndefined()) {
      throw Error("Must be defined\n", ErrorCode::BadParameter);
    }
  } catch (Error err) {
    throw Error("Bad parameter:" + parameter.parameter_name + "\n" + err.message,
                ErrorCode::BadParameter);
  };
};

//------------------------------------------------------------
//
//------------------------------------------------------------
class BaseParameter {
 public:
  bool isUndefined() const {
    return paramter_undefined;
  }
  bool isDefined() const {
    return !paramter_undefined;
  }

 protected:
  bool paramter_undefined = false;
};
//------------------------------------------------------------
struct CodeHash;

class IATACode : public BaseParameter {
 public:
  IATACode(std::string code);
  IATACode(uint32_t dbcode);
  uint32_t get_code() const;

 private:
  uint32_t code = 0;

  friend bool operator==(const IATACode& d1, const IATACode& d2);
  friend CodeHash;
};
bool operator==(const IATACode& d1, const IATACode& d2);

//------------------------------------------------------------
class CountryCode : public BaseParameter {
 public:
  CountryCode(std::string code);
  CountryCode(uint8_t code);
  uint8_t get_code() const;

 private:
  uint8_t code = 0;

  friend bool operator==(const CountryCode& d1, const CountryCode& d2);
  friend CodeHash;
};
bool operator==(const CountryCode& d1, const CountryCode& d2);
//------------------------------------------------------------
struct CodeHash {
  std::size_t operator()(IATACode const& obj) const {
    return int32hash(obj.code);
  }
  std::size_t operator()(CountryCode const& obj) const {
    return int8hash(obj.code);
  }

 private:
  std::hash<uint32_t> int32hash{};
  std::hash<uint8_t> int8hash{};
};

class IATACodes : public BaseParameter {
 public:
  IATACodes(std::string code);
  void add_code(uint32_t dbcode);
  const std::unordered_set<IATACode, CodeHash>& get_codes() const;

 private:
  std::unordered_set<IATACode, CodeHash> codes;
};

class CountryCodes : public BaseParameter {
 public:
  CountryCodes(std::string code);
  void add_code(uint8_t dbcode);
  const std::unordered_set<CountryCode, CodeHash>& get_codes() const;

 private:
  std::unordered_set<CountryCode, CodeHash> codes;
};
//------------------------------------------------------------
class Date : public BaseParameter {
 public:
  Date(std::string date);
  uint32_t days_after(Date const date) const;
  uint8_t day_of_week() const;
  uint32_t get_code() const;
  uint16_t get_year() const;
  uint8_t get_month() const;
  uint8_t get_day() const;

 private:
  uint32_t code = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint16_t year = 0;

  friend bool operator<(const Date& d1, const Date& d2);
  friend bool operator<=(const Date& d1, const Date& d2);
  friend bool operator>(const Date& d1, const Date& d2);
  friend bool operator>=(const Date& d1, const Date& d2);
  friend bool operator==(const Date& d1, const Date& d2);
};

bool operator<(const Date& d1, const Date& d2);
bool operator<=(const Date& d1, const Date& d2);
bool operator>(const Date& d1, const Date& d2);
bool operator>=(const Date& d1, const Date& d2);
bool operator==(const Date& d1, const Date& d2);
//------------------------------------------------------------
class Weekdays : public BaseParameter {
 public:
  Weekdays(std::string wd);
  Weekdays(Date date);
  uint8_t get_bitmask() const;

 private:
  uint8_t bitmask = 0;
};
//------------------------------------------------------------
class Number : public BaseParameter {
 public:
  Number(std::string number);
  Number(uint32_t number);
  uint32_t get_value() const;

 private:
  uint32_t value = 0;
};
//------------------------------------------------------------
class Boolean : public BaseParameter {
 public:
  Boolean(std::string val);
  Boolean(bool val);
  bool isTrue() const;
  bool isFalse() const;

 private:
  bool value = false;
};

//------------------------------------------------------------
// Converters
//------------------------------------------------------------
union PlaceCodec {
  uint32_t int_code;
  char iata_code[4];
};

uint32_t origin_to_code(std::string code);
std::string code_to_origin(uint32_t code);
uint32_t date_to_int(std::string date);
std::string int_to_date(uint32_t date);
uint8_t weekdays_bitmask(std::string days_of_week);
uint8_t country_to_code(std::string country);
std::string code_to_country(uint8_t code);

const std::array<const char[3], 252> COUNTRIES = {
    {"AF", "AX", "AL", "DZ", "AS", "AD", "AO", "AI", "AQ", "AG", "AR", "AM", "AW", "AU", "AT", "AZ",
     "BS", "BH", "BD", "BB", "BY", "BE", "BZ", "BJ", "BM", "BT", "BO", "BQ", "BA", "BW", "BV", "BR",
     "IO", "VG", "BN", "BG", "BF", "BI", "KH", "CM", "CA", "CV", "KY", "CF", "TD", "CL", "CN", "CX",
     "CC", "CO", "KM", "CK", "CR", "HR", "CU", "CW", "CY", "CZ", "CD", "DK", "DJ", "DM", "DO", "TL",
     "EC", "EG", "SV", "GQ", "ER", "EE", "ET", "FK", "FO", "FJ", "FI", "FR", "GF", "PF", "TF", "GA",
     "GM", "GE", "DE", "GH", "GI", "GR", "GL", "GD", "GP", "GU", "GT", "GG", "GN", "GW", "GY", "HT",
     "HM", "HN", "HK", "HU", "IS", "IN", "ID", "IR", "IQ", "IE", "IM", "IL", "IT", "CI", "JM", "JP",
     "JE", "JO", "KZ", "KE", "KI", "XK", "KW", "KG", "LA", "LV", "LB", "LS", "LR", "LY", "LI", "LT",
     "LU", "MO", "MK", "MG", "MW", "MY", "MV", "ML", "MT", "MH", "MQ", "MR", "MU", "YT", "MX", "FM",
     "MD", "MC", "MN", "ME", "MS", "MA", "MZ", "MM", "NA", "NR", "NP", "NL", "AN", "NC", "NZ", "NI",
     "NE", "NG", "NU", "NF", "KP", "MP", "NO", "OM", "PK", "PW", "PS", "PA", "PG", "PY", "PE", "PH",
     "PN", "PL", "PT", "PR", "QA", "CG", "RE", "RO", "RU", "RW", "BL", "SH", "KN", "LC", "MF", "PM",
     "VC", "WS", "SM", "ST", "SA", "SN", "RS", "CS", "SC", "SL", "SG", "SX", "SK", "SI", "SB", "SO",
     "ZA", "GS", "KR", "SS", "ES", "LK", "SD", "SR", "SJ", "SZ", "SE", "CH", "SY", "TW", "TJ", "TZ",
     "TH", "TG", "TK", "TO", "TT", "TN", "TR", "TM", "TC", "TV", "VI", "UG", "UA", "AE", "GB", "US",
     "UM", "UY", "UZ", "VU", "VA", "VE", "VN", "WF", "EH", "YE", "ZM", "ZW"}};

}  // namespace types
#endif