#include <iostream>
#include <sys/time.h>
#include <vector>

typedef long long timing_t;
timing_t get_timestamp_us() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  timing_t time_in_mill =
      (tv.tv_sec) * 1000 + (tv.tv_usec)/1000 ;

  return time_in_mill;
}

void test_genereal_array(uint32_t data_length){
	uint32_t data[data_length];
	uint32_t res = 0;

	timing_t start =  get_timestamp_us();

	for(int i = 0; i < data_length; i++){
		res += data[data_length];
	}

	timing_t end =  get_timestamp_us();	

	std::cout << "general array time:" << end - start << std::endl;
}

void test_new_array(uint32_t data_length){
	uint32_t* data = new uint32_t[data_length];
	uint32_t res = 0;

	timing_t start =  get_timestamp_us();

	for(int i = 0; i < data_length; i++){
		uint32_t d = data[data_length];
		res += d;
	}

	timing_t end =  get_timestamp_us();	

	std::cout << "new() array time:" << end - start << std::endl;
	delete data;
}

void test_ref_array(uint32_t data_length){
	uint32_t* data = new uint32_t[data_length];
	uint32_t res = 0;

	timing_t start =  get_timestamp_us();

	for(int i = 0; i < data_length; i++){
		uint32_t& d = data[data_length];
		res += d;
	}

	timing_t end =  get_timestamp_us();	

	std::cout << "ref array time:" << end - start << std::endl;
	delete data;
}



void test_vector_array(uint32_t data_length){
	std::vector<uint32_t> data;
	uint32_t res = 0;

	for(int i = 0; i < data_length; i++){
		data.push_back(i);
	}

	timing_t start =  get_timestamp_us();

	for(int i = 0; i < data_length; i++){
		res += data[data_length];
	}

	timing_t end =  get_timestamp_us();	

	std::cout << "vector array time:" << end - start << std::endl;
}

int main(){
	uint32_t size = 50000000;
	// test_genereal_array(size);
	test_new_array(size);
	test_ref_array(size);
	test_vector_array(size);

	return 0;
}