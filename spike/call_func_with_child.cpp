#include <iostream>

using namespace std;

template <typename Context>
class Connection{
public:
	Context context;
};

template <typename Type>
typedef void (ConnectionProcessor)(Connection<Type>& conn);


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

class B: public A {
public:
	B():A("B"){
		
	}
	string name;
};


int main(){
	// A a("1");
	B b;

	func1(b);
	
	cout << "finish\n";
}