#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <vector>

using namespace std;


struct test1
{
	int a;
	int b;
	char c[10];	
};

struct test2
{
	int a;
	union info {
		struct { 
			int b;
			char c[10];	
		} s;
		int d;
		int e;
		int f;
	} i;
};

int main(){
	test1 t1;
	test2 t2;

	cout << "test1:" << sizeof(t1) << endl;
	cout << "test2:" << sizeof(t2) << endl;

	cout << t1.a << t2.a << endl;
	cout << t1.b << t2.i.s.b << endl;

	return 0;
}
	