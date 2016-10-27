#include <iostream>

class RequestError {
 public:
  RequestError(std::string text, uint16_t code = 400) : message(text), code(code) {
  }

  const std::string message;
  const uint16_t code;
};

template <typename T, typename V>
class Parameter {
 public:
  Parameter() {
  }

  operator V() {
    return value;
  }

  const value;

 private:
};

class Date;
class IATA_Code;

int main() {
  Parameter<Date, std::string> t;
  try {
    std::cout << t.value() << std::endl;
  } catch (RequestError err) {
    std::cout << "ERROR:" << err.message << std::endl;
  }
  return 0;
}

// #define MAKE_PARAMETER(name, size)                                                            \
//   class name : public std::string {                                                           \
//    public:                                                                                    \
//     name(std::string a) : std::string(a) {                                                    \
//       check();                                                                                \
//     }                                                                                         \
//     name(const char a[]) : std::string(a) {                                                   \
//       check();                                                                                \
//     }                                                                                         \
//                                                                                               \
//    private:                                                                                   \
//     void check() {                                                                            \
//       if (this->length() != size) {                                                           \
//         throw RequestError(#name "(" #size ") wrong size:" + std::to_string(this->length())); \
//       }                                                                                       \
//     }                                                                                         \
//   };

// MAKE_PARAMETER(Test, 3);

// int main() {
//   try {
//     Test t{"asdR"};
//     std::cout << t << std::endl;
//   } catch (RequestError err) {
//     std::cout << "ERROR:" << err.message << std::endl;
//   }
//   return 0;
// }