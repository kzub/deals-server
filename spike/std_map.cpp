#include <iostream>
#include <map>

int main() {
  struct A {
    int a = 0;
    int b = 0;
  };
  std::map<int, A> test;

  A a;
  a.a = 1;
  A b;
  b.a = 2;
  A c;
  c.a = 3;

  auto &r = test[1];
  test[1] = a;
  test[1] = b;
  test[2] = c;

  A &d = a;
  r = d;
  r.a = 5;

  for (auto &m : test) {
    std::cout << m.first << "-" << m.second.a << " " << a.a << std::endl;
  }
  return 0;
}