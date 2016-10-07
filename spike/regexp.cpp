#include <array>
#include <iostream>

using namespace std;

int main() {
  std::array<int, 10> arr;
  arr.fill(10);

  for (auto& k : arr) {
    cout << k << endl;
  }
  return 0;
}