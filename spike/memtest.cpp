#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <vector>

using namespace std;

/*
 ORIGIN - DEPARTURE_MAP
 23*23*23 fixed array for instant search

 DEPARTUURE_MAP vectors of 1000 arrays
 *vector destroyed if last element is outdated
 *vector checked if first elemtn is outdated

 shared memory for processing speedup 
*/



struct row_type
{
	uint32_t timestamp;
	uint32_t origin;
	uint32_t destination;
	uint32_t departure_date;
	uint32_t return_date;
	uint8_t	flags; // direct?
	uint8_t  month;
	uint16_t stay;
	uint32_t price;
};

typedef struct row_type Data;

#define DATA_SIZE 5*1000*1000
// #define DATA_SIZE 5


void checkData(Data *datas, unsigned int portion_size, bool timing = 0){
	clock_t start = clock();

 	for(unsigned long i = 0; i < portion_size; i++){
 		if(datas[i].origin == 3){
 			cout << "oops";
 		}
 		if(datas[i].destination == 32){
 			cout << "oops";
 		}
 		if(datas[i].departure_date > 12 && datas[i].departure_date <  22){
 			cout << "oops";
 		}
 		if(datas[i].return_date > 132 && datas[i].return_date < 232){
 			cout << "oops";
 		}
 		if(datas[i].return_date - datas[i].departure_date == 2){
 			cout << "oops";
 		}
 		if(datas[i].stay > 2){
 			cout << "oops";
 		}
 	}	

 	if(timing){
		clock_t diff = clock() - start;
		int msec = diff * 1000 / CLOCKS_PER_SEC;
	  cout << "checkData() took " << msec << " ms\n";
	}
}

void checkVector(vector<Data*> myvector, unsigned int portion_size){
	clock_t start = clock();

	for (vector<Data*>::iterator it = myvector.begin() ; it != myvector.end(); ++it){
    checkData(*it, portion_size);
  }

  clock_t diff = clock() - start;
	int msec = diff * 1000 / CLOCKS_PER_SEC;
  cout << "checkVector() took " << msec << " ms\n";
}

int main(){
	unsigned int portion_size = 1000000;
	unsigned int portions = DATA_SIZE/portion_size ;
  Data *datas = new Data[DATA_SIZE];

	vector<Data*> myvector;
  for(unsigned int i = 0; i < portions; i++){
  	Data* test = new Data[portion_size];
  	myvector.push_back(test);
  }

  cout << "capacity:" << myvector.capacity() << " size:" << myvector.size() << "\n";
  cout << "memory:" << sizeof(Data)*portion_size*myvector.size() << " sizeofData:" << sizeof(Data) << endl;

// while(1){
  checkVector(myvector, portion_size);
 	// checkData(datas, DATA_SIZE, true);
// }
  checkVector(myvector, portion_size);
  checkVector(myvector, portion_size);
  checkVector(myvector, portion_size);
  checkVector(myvector, portion_size);
 	
 	checkData(datas, DATA_SIZE, true);
 	checkData(datas, DATA_SIZE, true);
 	checkData(datas, DATA_SIZE, true);
 	checkData(datas, DATA_SIZE, true);
 	return 0;
}
	