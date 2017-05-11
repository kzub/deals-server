#include <iostream>

// using namespace std;

class Test {
 public:
  Test() {
    std::cout << "Test zero:" << std::endl;
    s = "zero";
  }

  Test(std::string s) : s(s) {
    std::cout << "Test:" << s << std::endl;
  }

  ~Test() {
    std::cout << "~Test:" << s << " count:" << count << std::endl;
  }

  Test(Test&& t) {
    std::cout << "Test&& " << s << std::endl;
    s = "moved_" + t.s;
  }

  Test(Test& t) {
    s = "ref_" + t.s;
    std::cout << "Test& " << s << std::endl;
  }

  Test& operator=(Test&& t) {
    std::cout << "Test operator= " << s << std::endl;
    s = "assignmoved_" + t.s;
    return *this;
  }

  std::string s;
  int count = 0;
  void print() {
    std::cout << "print:" << s << " count:" << ++count << std::endl;
  }
};

Test&& test() {
  Test k("F");
  k.print();
  return std::move(k);
}

int main() {
  // Test* t = 0;
  // try {
  //   t = new Test("T");
  // } catch (const char* msg) {
  //   std::cout << "catch:" << msg << std::endl;
  // }
  // std::cout << "mem:" << (void*)t << std::endl;
  // t->print();

  Test t("1");
  // Test f;
  // f = std::move(t);
  // f = std::move(test());
  t = std::move(test());
  t.print();

  std::cout << "*finish*\n";
}