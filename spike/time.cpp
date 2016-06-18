#include <iostream>
#include <cinttypes>
#include <cassert>

// ISO date standare 2016-06-16
uint32_t date_to_int(std::string date){
	if(date.length() != 10){ return 0; }
	if(date[4] != '-' || date[7] != '-'){ return 0; }
	
	date.erase(4, 1); 
	date.erase(6, 1);
	
	try{
		return std::stol(date);
	}catch (std::exception e){
		return 0;
	}
};

std::string int_to_date(uint32_t date){
	if(!date){ return ""; }

	std::string result;
	uint16_t year;
	uint16_t month;
	uint16_t day;

	// 20160601
	year = date / 10000;
	month = (date - year * 10000) / 100;
	day = date - year * 10000 - month * 100;

	result = std::to_string(year) + "-" + 
			(month < 10 ? "0" : "") + 
			std::to_string(month) + "-" +
			(day < 10 ? "0" : "") + 
			std::to_string(day);
	return result;
};


int main(){

	uint32_t code = date_to_int("2017-01-01");
	std::string date = int_to_date(code);

	assert(code == 20170101);
	assert(date == "2017-01-01");
	
	std::cout << "finish: " << code << " : " << date << std::endl;
}