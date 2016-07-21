#include <cassert>

#include <stdio.h>
#include <sys/statvfs.h>
#include "shared_memory.hpp"
#include "timing.hpp"
namespace shared_mem {

//-----------------------------------------------------------
// Check system has free shared memory
//-----------------------------------------------------------
bool checkSharedMemAvailability() {
  struct statvfs res;
  statvfs("/dev/shm/", &res);

  uint32_t freemem = (100 * res.f_bavail / res.f_blocks);

  if (freemem <= 5) {
    std::cout << "WARNGING LOW MEMORY:" << freemem << "%" << std::endl;
  }
  std::cout << "MEMORY:" << freemem << "%" << std::endl;

  // pages will not allocated by 'new SharedMemory()'
  return freemem > 3;
}

/* ----------------------------------------------------------
**  TESTING......
** ----------------------------------------------------------*/
struct TestInfo {
  uint32_t value;
};
// ---------------------------------------------------------

/* ----------------------------------------------------------
**  Check results
** ----------------------------------------------------------*/
class TestResult : public TableProcessor<TestInfo> {
 public:
  TestResult(Table<TestInfo>* table) : table(table) {
  }

  bool process_function(TestInfo* elements, uint32_t size) {
    // std::cout << "size:" << size << std::endl;
    uint32_t idx;
    for (idx = 0; idx < size; ++idx) {
      if (found.size() <= elements[idx].value) {
        for (uint32_t todo = elements[idx].value - found.size() + 1; todo > 0; todo--) {
          // std::cout << "PUSHBACK size:" << found.size() << " value:" <<
          // elements[idx].value << std::endl;
          found.push_back(0);
        }
      }

      found[elements[idx].value]++;
      // std::cout << " idx:" << idx << " value:" << elements[idx].value << "
      // found:" << found[elements[idx].value] <<std::endl;
    }
    // std::cout << std::endl << "found:" << found << std::endl;
    return true;
  }

  void go() {
    table->processRecords(*this);
  }

  Table<TestInfo>* table;
  std::vector<uint32_t> found;
};

// ---------------------------------------------------------
std::vector<uint32_t> check(Table<TestInfo>& index) {
  TestResult scan_result(&index);
  scan_result.go();

  std::cout << "RESULT(" << scan_result.found.size() << "): ";
  for (uint32_t idx = 0; idx < scan_result.found.size(); ++idx) {
    std::cout << scan_result.found[idx] << " ";
  }
  std::cout << std::endl;

  return scan_result.found;
}
// ---------------------------------------------------------

void testAddMultipleRecords(Table<TestInfo>* t, uint32_t number, uint8_t numval = 0,
                            uint32_t lifetime = 0) {
  uint32_t idx;
  for (idx = 0; idx < number; ++idx) {
    TestInfo test = {numval};
    ElementPointer<TestInfo> result = t->addRecord(&test, 1, lifetime);

    if (result.error != ErrorCode::NO_ERROR) {
      std::cout << "ERROR testAddMultipleRecords:" << (int)result.error << std::endl;
    } else {
      // std::cout << "OK:" << result.size << std::endl;
    }
    assert(result.error == ErrorCode::NO_ERROR);
  }
  // std::cout << "ADDED " << idx << " records" << std::endl;
}

int unit_test() {
  Table<TestInfo> index("TT", 1000, 100, 60);
  index.cleanup();
  /* ----------------------------------------------------------
  **  Fill table
  ** ----------------------------------------------------------*/
  testAddMultipleRecords(&index, 80 /*elements*/, 1 /*value*/, 5 /*expire*/);
  testAddMultipleRecords(&index, 20, 1, 2);

  testAddMultipleRecords(&index, 100, 2, 2);
  testAddMultipleRecords(&index, 100, 3, 4);
  testAddMultipleRecords(&index, 100, 5, 10);
  std::cout << "BLOCK 1 -------------->" << std::endl;

  timing::TimeLord seconds(10 /* ticks in one second */);

  for (seconds.reset(); seconds.test(8); ++seconds) {
    std::vector<uint32_t> res = check(index);

    assert(res[0] == 0);
    assert(res[4] == 0);

    if (seconds <= 5)
      assert(res[1] == 100);
    else
      assert(res[1] == 0);

    if (seconds <= 2)
      assert(res[2] == 100);
    else
      assert(res[2] == 0);

    if (seconds <= 4)
      assert(res[3] == 100);
    else
      assert(res[3] == 0);

    if (seconds <= 10)
      assert(res[5] == 100);
  }

  testAddMultipleRecords(&index, 50, 2, 2);
  testAddMultipleRecords(&index, 50, 3, 2);
  testAddMultipleRecords(&index, 150, 4, 15);

  std::cout << "BLOCK 2 -------------->" << std::endl;

  for (seconds.reset(); seconds.test(5); ++seconds) {
    std::vector<uint32_t> res = check(index);

    assert(res[0] == 0);
    assert(res[1] == 0);
    assert(res[4] == 150);

    if (seconds <= 2)
      assert(res[5] == 100);
    else
      assert(res.size() == 5);

    if (seconds <= 2)
      assert(res[2] == 50);
    else
      assert(res[2] == 0);

    if (seconds <= 2)
      assert(res[3] == 50);
    else
      assert(res[3] == 0);
  }

  testAddMultipleRecords(&index, 100, 1, 2);
  testAddMultipleRecords(&index, 150, 1, 2);

  std::cout << "BLOCK 3 -------------->" << std::endl;

  for (seconds.reset(); seconds.test(20); ++seconds) {
    std::vector<uint32_t> res = check(index);

    if (seconds < 11) {
      assert(res.size() > 0);
      assert(res[0] == 0);
      assert(res[2] == 0);
      assert(res[3] == 0);
      assert(res[4] == 150);
    } else
      assert(res.size() == 0);
  }

  std::cout << "TEST: OK" << std::endl;

  return 0;
}
}

int mainss() {
  shared_mem::unit_test();
  return 0;
}
