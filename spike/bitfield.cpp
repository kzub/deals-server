#include <iostream>

struct test {
  bool a : 1;
  bool b : 1;
  bool c : 1;
  uint8_t i : 4;
} t;

int main() {
  uint8_t a = 2;
  // t.i = 9;

  std::cout << (int)t.i << (int)a << (int)((1 << a) & t.i) << std::endl;

  t.a = 0;
  t.c = 0;
  t.b = 4;
  std::cout << "test:" << t.a << t.b << t.c << std::endl;
  std::cout << sizeof(t) << std::endl;
}