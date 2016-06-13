#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <vector>

using namespace std;


int main(){

	string test;

	uint8_t bytes[] = "1234\00056789\0001011121314";

	test.append((const char*)bytes, sizeof(bytes));

	cout << "\n" << test << "\n" << test.length() << endl;

	// try{
		uint32_t price_int = std::stoi("d2323") notrhow;
		std::cout << "price:" << price_int << std::endl;		
	// }catch(exception e){

	// }

	return 0;
}
	