#include <iostream>

// using namespace std;

class Test {
 public:
  Test(std::string s) : s(s) {
    throw "kak";
    std::cout << "Test:" << s << std::endl;
  }

  ~Test() {
    std::cout << "~Test:" << s << std::endl;
  }

  Test(Test&& t) {
    std::cout << "Test&& " << s << std::endl;
    s = "moved_" + t.s;
  }

  Test& operator=(Test&& t) {
    std::cout << "Test operator&& " << s << std::endl;
    s = "assignmoved_" + t.s;
    return *this;
  }

  Test(Test& t) {
    s = "copied_" + t.s;
    std::cout << "Test& " << s << std::endl;
  }

  std::string s;
  int count = 0;

  void print() {
    std::cout << "print:" << s << ":" << count++ << std::endl;
  }
};

Test test() {
  Test k("F");
  k.print();
  return k;
}

int main() {
  Test* t = 0;
  try {
    t = new Test("T");
  } catch (...) {
    std::cout << "catch!" << std::endl;
  }

  std::cout << "mem:" << (void*)t << std::endl;
  //  t->print();

  // t = test();
  // Test t("1");
  // Test f = std::move(t);
  std::cout << "*finish*\n";
}