#include <iostream>

template <typename Type>
class PrimaryStrongType {
 public:
  PrimaryStrongType(Type v) : value(v) {
  }
  operator Type() {
    return value;
  }
  Type value;
};

template <typename Type>
class StrongType : public Type {
 public:
  StrongType(Type init) : Type(init) {
  }
};
// using IATACode = StrongType;

using Date = PrimaryStrongType<int>;
using IATACode1 = StrongType<std::string>;
using IATACode2 = StrongType<std::string>;

void test(IATACode1 code1, IATACode2 code2) {
  std::cout << code1 << "-" << code2 << std::endl;
}

int main() {
  Date d = 3;
  // Date<std::string> s = {"dd"};
  // std::string s = "dd";
  IATACode1 d1 = {"one"};
  IATACode2 d2 = {"two"};
  test(d1, d1);
  // std::cout << d << s << std::endl;
  return 0;
}