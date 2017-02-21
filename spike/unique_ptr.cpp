#include <iostream>

struct Example {
  std::string p1;
  std::string p2;
};

void test(Example& ex) {
  std::cout << "test: " << ex.p1 << std::endl;
}

int main() {
  Example ex{"1", "2"};
  std::unique_ptr<Example> ptr(new Example{"1", "2"});
  std::cout << "finish:" << ex.p1 << " " << ex.p2 << std::endl;
  std::cout << "finish:" << ptr->p1 << " " << ptr->p2 << std::endl;

  Example* ex2 = &ex;
  test(ex2);
}