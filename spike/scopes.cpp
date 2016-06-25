#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <vector>

using namespace std;

class Test {
 public:
  Test() {
    cout << "constructor" << endl;
  }
  ~Test() {
    cout << "destructor" << endl;
  }

  Test* operator->() {
    cout << "!!!operator!!!" << endl;
    return this;
  };

  Test& operator*() {
    cout << "!!!operator!!!" << endl;
    return *this;
  };

  Test* operator&() {
    cout << "!!!operator!!!" << endl;
    return this;
  };

  void print() {
    cout << "--print" << endl;
  }
};

void func2(Test* t) {
  cout << "f2" << endl;
  t->print();
}

void func1() {
  // Test *t = new Test();
  Test t;
  func2(&t);
}

int main() {
  // while(1){
  func1();
  // }

  cout << "sss" << ::endl;
  return 0;
}
