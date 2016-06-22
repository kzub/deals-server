#include <iostream>


struct test {
	bool a : 1;
	bool b : 1;
	bool c : 1;
	bool d : 1;
	bool e : 1;
	bool f : 1;
} t;

int main(){
	t.a = 5;
	t.b = true;
	std::cout << "test:" << t.a << t.b << t.c << std::endl;
	std::cout << sizeof(t) << std::endl;
}