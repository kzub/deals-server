#include "iostream"

enum class Result : char { OK = 3, ERR1, ERR2 };

int main() {
  Result f = Result::OK;
  std::cout << (int)f << std::endl;
  return 0;
}