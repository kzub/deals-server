#include <iostream>

template <typename Base>
class MayBe : public Base {
 public:
  template <typename... Args>
  MayBe(std::string name, Args&&... args) try : Base(std::forward<Args>(args)...) {
  } catch (...) {
    throw "Bad parameter:" + name + "\n";
  };
};

template <typename Base>
class MustBe : public Base {
 public:
  template <typename... Args>
  MustBe(std::string name, Args&&... args) try : Base(std::forward<Args>(args)...) {
    if (this->isUndefined()) {
      throw "Bad parameter:" + name + "\n";
    }
  } catch (...) {
    throw "Bad parameter:" + name + "\n";
  };
};

class Test {
 public:
  Test(std::string p) {
    val = p;
    throw "ddd";
  }
  std::string value() {
    return val;
  }
  bool isUndefined() {
    return true;
  }

 private:
  std::string val;
};

int main() {
  try {
    MayBe<Test> test("name1", "testvalue");
    std::cout << "OK " << test.value() << std::endl;

  } catch (std::string t) {
    std::cout << "error:" << t << std::endl;
  }
  return 0;
}