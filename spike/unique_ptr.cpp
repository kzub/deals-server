#include <iostream>

struct Example {
  std::string p1;
  std::string p2;
};

int main() {
  Example ex{"1", "2"};
  std::unique_ptr<Example> ptr(new Example{"1", "2"});
  std::cout << "finish:" << ex.p1 << " " << ex.p2 << std::endl;
  std::cout << "finish:" << ptr->p1 << " " << ptr->p2 << std::endl;
}