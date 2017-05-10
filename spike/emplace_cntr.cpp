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
    s = "moved_" + t.s;
    std::cout << "Test&& " << s << std::endl;
  }

  Test(const Test& t) {
    s = "ref_" + t.s;
    std::cout << "Test& " << s << std::endl;
  }

  Test& operator=(Test&& t) {
    s = "assignmoved_" + t.s;
    std::cout << "Test operator= " << s << std::endl;
    return *this;
  }

  std::string s;
  int count = 0;
  void print(std::string msg = "") {
    std::cout << "print:" << msg << " s:" << s << " count:" << ++count << std::endl;
  }
};

void test(Test k) {
  Test fwd(std::forward<Test>(k));
  // Test fwd(k);
  fwd.print();
}

int main() {
  test({"M"});

  Test k("k");
  test(std::move(k));
  std::cout << "*finish*\n";
}
