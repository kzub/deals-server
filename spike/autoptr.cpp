#include <iostream>

using namespace std;

class A { public:
	A(string name) : name(name) {
		
		cout << "A(" << name << ")" << endl;
	}
	~A() {
		
		cout << "~A(" << name << ")" << endl;
	}

	A (const A &obj) : name("copyof:"+ obj.name) {
	  cout << "copy constructor(" << name << ")" << endl;
	}

	string name;
};

void func1(A b){
	b.name = "modidy";
	cout << "func1:" << b.name << endl;
}

A func2(){
	cout << "func2" << endl;
	A c("2");
	return c;
}

int main(){
	A a("1");

	func1(a);
	A d = func2();

	
	cout << "finish\n";
}