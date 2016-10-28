#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
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

#define DATA_SIZE 5 * 1000 * 1000
struct Data {
  uint32_t timestamp;
  uint32_t origin;
  uint32_t destination;
  uint32_t departure_date;
  uint32_t return_date;
  uint8_t flags;  // direct?
  uint8_t month;
  uint16_t stay;
  uint32_t price;
};

void checkData(Data *datas, unsigned int portion_size, bool timing = 0) {
  clock_t start = clock();

  for (unsigned long i = 0; i < portion_size; i++) {
    auto data = datas[i];

    if (data.origin == 3) {
      cout << "oops";
    }
    if (data.destination == 32) {
      cout << "oops";
    }
    if (data.departure_date > 12 && data.departure_date < 22) {
      cout << "oops";
    }
    if (data.return_date > 132 && data.return_date < 232) {
      cout << "oops";
    }
    if (data.return_date - data.departure_date == 2) {
      cout << "oops";
    }
    if (data.stay > 2) {
      cout << "oops";
    }
  }

  if (timing) {
    clock_t diff = clock() - start;
    int msec = diff * 1000 / CLOCKS_PER_SEC;
    cout << "checkData() took " << msec << " ms\n";
  }
}

void checkVector(vector<Data *> myvector, unsigned int portion_size) {
  clock_t start = clock();

  for (const auto &data : myvector) {
    checkData(data, portion_size);
  }

  clock_t diff = clock() - start;
  int msec = diff * 1000 / CLOCKS_PER_SEC;
  cout << "checkVector() took " << msec << " ms\n";
}

int main() {
  unsigned int portion_size = 1000000;
  unsigned int portions = DATA_SIZE / portion_size;
  Data *datas = new Data[DATA_SIZE];

  vector<Data *> myvector;
  for (auto i = 0; i < portions; i++) {
    Data *test = new Data[portion_size];
    myvector.push_back(test);
  }

  cout << "capacity:" << myvector.capacity() << " size:" << myvector.size() << "\n";
  cout << "memory:" << sizeof(Data) * portion_size * myvector.size()
       << " sizeofData:" << sizeof(Data) << endl;

  checkVector(myvector, portion_size);
  checkVector(myvector, portion_size);
  checkVector(myvector, portion_size);
  checkVector(myvector, portion_size);
  checkVector(myvector, portion_size);

  checkData(datas, DATA_SIZE, true);
  checkData(datas, DATA_SIZE, true);
  checkData(datas, DATA_SIZE, true);
  checkData(datas, DATA_SIZE, true);
  checkData(datas, DATA_SIZE, true);
  return 0;
}
