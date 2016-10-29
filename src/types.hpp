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
    throw Error("Bad parameter:" + name + "\n" + err.message, ErrorCode::BadParameter);
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
    throw Error("Bad parameter:" + name + "\n" + err.message, ErrorCode::BadParameter);
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

const std::array<const char[3], 243> COUNTRIES = {
    {"AD", "AE", "AF", "AG", "AI", "AL", "AM", "AN", "AO", "AQ", "AR", "AS", "AT", "AU", "AW", "AX",
     "AZ", "BA", "BB", "BD", "BE", "BF", "BG", "BH", "BI", "BJ", "BM", "BN", "BO", "BR", "BS", "BT",
     "BV", "BW", "BY", "BZ", "CA", "CC", "CD", "CF", "CG", "CH", "CI", "CK", "CL", "CM", "CN", "CO",
     "CR", "CS", "CU", "CV", "CX", "CY", "CZ", "DE", "DJ", "DK", "DM", "DO", "DZ", "EC", "EE", "EG",
     "EH", "ER", "ES", "ET", "FI", "FJ", "FK", "FM", "FO", "FR", "GA", "GB", "GD", "GE", "GF", "GG",
     "GH", "GI", "GL", "GM", "GN", "GP", "GQ", "GR", "GS", "GT", "GU", "GW", "GY", "HK", "HM", "HN",
     "HR", "HT", "HU", "ID", "IE", "IL", "IM", "IN", "IO", "IQ", "IR", "IS", "IT", "JE", "JM", "JO",
     "JP", "KE", "KG", "KH", "KI", "KM", "KN", "KP", "KR", "KW", "KY", "KZ", "LA", "LB", "LC", "LI",
     "LK", "LR", "LS", "LT", "LU", "LV", "LY", "MA", "MC", "MD", "MG", "MH", "MK", "ML", "MM", "MN",
     "MO", "MP", "MQ", "MR", "MS", "MT", "MU", "MV", "MW", "MX", "MY", "MZ", "NA", "NC", "NE", "NF",
     "NG", "NI", "NL", "NO", "NP", "NR", "NU", "NZ", "OM", "PA", "PE", "PF", "PG", "PH", "PK", "PL",
     "PM", "PN", "PR", "PS", "PT", "PW", "PY", "QA", "RE", "RO", "RU", "RW", "SA", "SB", "SC", "SD",
     "SE", "SG", "SH", "SI", "SJ", "SK", "SL", "SM", "SN", "SO", "SR", "ST", "SV", "SY", "SZ", "TC",
     "TD", "TF", "TG", "TH", "TJ", "TK", "TL", "TM", "TN", "TO", "TR", "TT", "TV", "TW", "TZ", "UA",
     "UG", "UM", "US", "UY", "UZ", "VA", "VC", "VE", "VG", "VI", "VN", "VU", "WF", "WS", "YE", "YT",
     "ZA", "ZM", "ZW"}};

}  // namespace types
#endif