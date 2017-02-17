#include <cassert>

#include <sys/statvfs.h>

#include "shared_memory.hpp"
#include "timing.hpp"

namespace shared_mem {
//-----------------------------------------------------------
// Check system has free shared memory
//-----------------------------------------------------------
bool checkSharedMemAvailability() {
#ifdef __APPLE__  // doesnt work on apple
  return true;
#endif
  struct statvfs res;
  statvfs("/dev/shm/", &res);
  uint32_t freemem = 100 * res.f_bavail / res.f_blocks;

  if (freemem <= LOWMEM_ERROR_PERCENT) {
    std::cerr << "ERROR VERY LOW MEMORY:" << freemem << "%" << std::endl;
    return false;
  }
  std::cout << "MEMORY1:" << freemem << "%" << std::endl;
  return true;
}

bool isLowMem() {  // cache??
#ifdef __APPLE__   // doesnt work on apple
  return false;
#endif
  struct statvfs res;
  statvfs("/dev/shm/", &res);
  uint32_t freemem = 100 * res.f_bavail / res.f_blocks;

  if (freemem <= LOWMEM_ERROR_PERCENT) {
    std::cerr << "WARNGING LOW MEMORY:" << freemem << "%" << std::endl;
    return true;
  }
  std::cout << "MEMORY2:" << freemem << "%" << std::endl;
  return false;
}

/* ----------------------------------------------------------
**  TESTING......
** ----------------------------------------------------------*/
struct TestInfo {
  uint32_t value;
};
// ---------------------------------------------------------

/* ----------------------------------------------------------
**  TestResult            Check results
** ----------------------------------------------------------*/
class TestResult : public TableProcessor<TestInfo> {
 public:
  TestResult(Table<TestInfo>* table) : table(table) {
  }

  void process_element(const TestInfo& element) {
    if (found.size() <= element.value) {
      for (uint32_t todo = element.value - found.size() + 1; todo > 0; todo--) {
        // std::cout << "PUSHBACK size:" << found.size() << " value:" <<
        // element.value << std::endl;
        found.push_back(0);
      }
    }

    found[element.value]++;
  }

  void go() {
    table->processRecords(*this);
  }

  Table<TestInfo>* table;
  std::vector<uint32_t> found;
};

//---------------------------------------------------------
// Test::check
//---------------------------------------------------------
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

//---------------------------------------------------------
// Test::testAddMultipleRecords
//---------------------------------------------------------
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

//---------------------------------------------------------
// Test::unit_test
//---------------------------------------------------------
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
}  // namespace shared_mem

int mainss() {
  shared_mem::unit_test();
  return 0;
}
