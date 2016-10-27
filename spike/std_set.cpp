#include <iostream>
#include <unordered_set>

struct A {
  std::string a = "";
  int b = 0;
};
bool operator==(const A &a, const A &b) {
  return a.a == b.a && a.b == b.b;
}

struct MyHash {
  std::size_t operator()(A const &a) const {
    std::size_t h1 = test(a.a);
    return h1;
  }
  std::hash<std::string> test = {};
};

int main() {
  // std::hash Ah;
  std::unordered_set<A, MyHash> codes;
  A a{"p1", 1};
  A b{"p2", 3};
  A c{"p3", 3};
  codes.insert(a);
  codes.insert(b);
  codes.insert(c);
  codes.emplace(std::move((A){"p4", 4}));

  for (auto &code : codes) {
    std::cout << "code:" << code.a << std::endl;
  }
  return 0;
}