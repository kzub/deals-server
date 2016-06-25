#include <iostream>

static std::string days[] = {"", "mon", "tue", "wed", "thu", "fri", "sat", "sun"};
// http://www.geeksforgeeks.org/find-day-of-the-week-for-a-given-date/
uint8_t day_of_week(uint8_t d, uint8_t m, uint16_t y)
{	
    static uint8_t t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    y -= m < 3;
    uint8_t res = (( y + y/4 - y/100 + y/400 + t[m-1] + d) % 7);
    if(res == 0){
    	return 7;
    }
    return res;
}

uint8_t day_of_week_from_date(std::string date){
	if(date[4] != '-' || date[7] != '-'){
		return 0;
	}
	std::string year = date.substr(0, 4);
	std::string month = date.substr(5, 7);
	std::string day = date.substr(8, 10);

	uint16_t int_year = 0;
	uint8_t int_month = 0;
	uint8_t int_day = 0;

	try{
		int_year = std::stoi(year);
		int_month = std::stoi(month);
		int_day = std::stoi(day);
	} catch(...){
		return 0;
	}

	return day_of_week(int_day, int_month, int_year);
}

std::string day_of_week_str_from_date(std::string date) {
	uint8_t day = day_of_week_from_date(date);
	return days[day];
}
	
uint8_t day_of_week_from_str(std::string weekday) {
	for(uint8_t i = 1; i <= 7; i++){
		if(days[i] == weekday){
			return i;
		}
	}
	return 0;
}

int main(){
	std::cout << day_of_week_str_from_date("2016-06-24") << std::endl;
	std::cout << (int)day_of_week_from_date("2016-06-24") << std::endl;
	std::cout << (int)day_of_week_from_str("sun") << std::endl;
	return 0;
}