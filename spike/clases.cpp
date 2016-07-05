#include <iostream>

class A {
 public:
  int a = 1;
};

class B : public A {
 public:
  int b = 2;
  void exec() {
    std::cout << "a:" << a << std::endl;
    this->process();
  }
  virtual void process() {
    std::cout << "process in B" << std::endl;
  }
};

class C : public B {
 public:
  void process() {
    std::cout << "process in C" << std::endl;
    // B::process();
  }
  int c = 3;
};

int main() {
  C c;
  c.exec();

  // std::cout << b.b << std::endl;
  return 0;
}