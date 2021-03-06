#include <cassert>

#include <sys/statvfs.h>

#include "shared_memory.hpp"
#include "statsd_client.hpp"
#include "timing.hpp"

namespace shared_mem {
//-----------------------------------------------------------
// Check that system has free shared memory
//-----------------------------------------------------------
bool isMemAvailable() {
#ifdef __APPLE__  // doesnt work on apple
  return true;
#endif
  static struct statvfs res;
  statvfs("/dev/shm/", &res);
  uint32_t freemem = 100 * res.f_bavail / res.f_blocks;

  if (freemem <= LOWMEM_ERROR_PERCENT) {
    std::cerr << "ERROR VERY LOW MEMORY:" << freemem << "%" << std::endl;
    return false;
  }
  return true;
}

//-----------------------------------------------------------
bool isMemLow() {
#ifdef __APPLE__  // doesnt work on apple
  return false;
#endif
  static struct statvfs res;
  statvfs("/dev/shm/", &res);
  uint32_t freemem = 100 * res.f_bavail / res.f_blocks;

  if (freemem <= LOWMEM_PERCENT_FOR_PAGE_REUSING) {
    return true;
  }
  return false;
}

//-----------------------------------------------------------
void reportMemUsage(const PageType current_record_type, const std::string& insert_page_name) {
  if (current_record_type == PageType::NEW) {
    std::cout << "USE NEW page:" << insert_page_name << std::endl;
    statsd::metric.inc("dealsrv.page_use", {{"page", "new"}});
  } else if (current_record_type == PageType::EXPIRED) {
    std::cout << "USE EXPIRED page:" << insert_page_name << std::endl;
    statsd::metric.inc("dealsrv.page_use", {{"page", "expired"}});
  } else if (current_record_type == PageType::OLDEST) {
    std::cout << "USE OLDEST page:" << insert_page_name << std::endl;
    statsd::metric.inc("dealsrv.page_use", {{"page", "oldest"}});
  }

#ifndef __APPLE__  // doesnt work on apple
  static struct statvfs res;
  statvfs("/dev/shm/", &res);
  uint32_t freemem = 100 * res.f_bavail / res.f_blocks;

  statsd::metric.gauge("dealsrv.shmem_free", freemem);
  std::cout << "(" << insert_page_name << ") MEMORY:" << std::to_string(freemem) << std::endl;
#endif
}

//-----------------------------------------------------
// SharedContext shared by all tables
//-----------------------------------------------------
SharedContext::SharedContext(std::string name)
    : data{name + "Context", 1}, shm{*data.getElements()} {
  std::cout << "SharedContext created: " << name << "SharedContext" << std::endl;
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
    auto result = t->addRecord(&test, 1, lifetime);
  }
}

//---------------------------------------------------------
// Test::unit_test
//---------------------------------------------------------
int unit_test() {
  SharedContext ctx{"TT"};
  Table<TestInfo> index("TT", 1000, 100, 60, ctx);
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
