#ifndef SRC_SHAREDMEM_HPP
#define SRC_SHAREDMEM_HPP

#include <sys/mman.h>
#include <cinttypes>
#include <functional>
#include <iostream>
#include <vector>

#include "locks.hpp"
#include "types.hpp"

namespace shared_mem {

#define MEMPAGE_NAME_MAX_LEN 20
#define MEMPAGE_REMOVE_EXPIRED_PAGES_AT_ONCE 5
#define MEMPAGE_REMOVE_EXPIRED_PAGES_DELAY_SEC 60
#define MEMPAGE_CHECK_EXPIRED_PAGES_INTERVAL_SEC 5
static_assert(MEMPAGE_REMOVE_EXPIRED_PAGES_DELAY_SEC > MEMPAGE_CHECK_EXPIRED_PAGES_INTERVAL_SEC,
              "CHECK MEM CLEAR SETTINGS");

#define LOWMEM_PERCENT_FOR_PAGE_REUSING 15
#define LOWMEM_ERROR_PERCENT 10
static_assert(LOWMEM_PERCENT_FOR_PAGE_REUSING > LOWMEM_ERROR_PERCENT, "CHECK LOWMEM SETTINGS");

template <typename ELEMENT_T>
class Table;
class SharedContext;

enum class PageType : int { EXPIRED, OLDEST, NEW, CURRENT, UNKNOWN };
bool isMemAvailable();
bool isMemLow();
void reportMemUsage(const PageType current_record_type, const std::string& insert_page_name);

//-------------------------------------------------------
// SharedMemoryPage
//-------------------------------------------------------
template <typename ELEMENT_T>
class SharedMemoryPage {
 public:
  ELEMENT_T* getElements();

 private:
  SharedMemoryPage(std::string page_name, uint32_t elements);
  ~SharedMemoryPage();

  // every shared memory page has this properties:
  struct Page_information {
    bool unlinked;
    uint32_t expiration_check;
  };
  Page_information* shared_pageinfo;

  // page property
  std::string page_name;
  uint32_t page_memory_size;

  // pointers to shared memory
  void* shared_memory;
  ELEMENT_T* shared_elements;

  static void unlink(std::string page_name) {
    std::cout << "UNLINK: " << page_name << std::endl;
    // The operation of shm_unlink() is analogious to unlink(2): it removes a
    // shared memory object name, and, once all processes have unmapped the
    // object, de-allocates and destroys the contents of the associated memory
    // region. http://linux.die.net/man/3/shm_open
    shm_unlink(page_name.c_str());
  }

  template <class T>
  friend class Table;
  friend class SharedContext;
};

//-----------------------------------------------
// DB Context
//-----------------------------------------------
struct DBContext {
  uint32_t global_expire_at;
  uint8_t reserved[1000];
};

class SharedContext {
 public:
  SharedContext(std::string name);

 private:
  SharedMemoryPage<DBContext> data;

 public:
  DBContext& shm;
};

//-----------------------------------------------
// TablePageIndexElement
//-----------------------------------------------
// information about all open pages in all processes
struct TablePageIndexElement {
  uint32_t expire_at;
  uint32_t page_elements_available;
  char page_name[MEMPAGE_NAME_MAX_LEN];
};

//-----------------------------------------------
// ElementExtractor
//-----------------------------------------------
template <typename ELEMENT_T>
class ElementExtractor {
 public:
  ElementExtractor(Table<ELEMENT_T>& table, std::string page_name, uint32_t index, uint32_t size)
      : page_name(page_name), index(index), size(size), table(table){};

  ELEMENT_T* get_element_data();

  const std::string page_name;
  const uint32_t index;
  const uint32_t size;

 private:
  Table<ELEMENT_T>& table;
};

//-----------------------------------------------
// TableProcessor
//-----------------------------------------------
template <typename ELEMENT_T>
class TableProcessor {
 protected:
  // function that will be called for iterating over all not expired pages in table
  virtual void process_element(const ELEMENT_T& element) = 0;

  template <class T>
  friend class Table;
};

//-----------------------------------------------
// Table
//-----------------------------------------------
template <typename ELEMENT_T>
class Table {
 public:
  Table(std::string table_name, uint16_t table_max_pages, uint32_t max_elements_in_page,
        uint32_t record_expire_seconds, SharedContext& context);
  ~Table();

  ElementExtractor<ELEMENT_T> addRecord(ELEMENT_T* el, uint32_t size = 1,
                                        uint32_t lifetime_seconds = 0);
  void processRecords(TableProcessor<ELEMENT_T>& result);
  void cleanup();
  const SharedContext context;

 private:
  SharedMemoryPage<ELEMENT_T>* localGetPageByName(const std::string& page_name_to_look);
  SharedMemoryPage<ELEMENT_T>* getPageByName(const std::string& page_name_to_look);
  void release_open_pages();
  void clear_index_record(TablePageIndexElement& record);
  void clear_index_record_full(TablePageIndexElement& record);
  void release_expired_memory_pages();
  void checkRecord(uint32_t& records_cout);
  void update_record_expire(TablePageIndexElement* index_record, uint32_t current_time,
                            uint32_t lifetime_seconds);
  void update_global_expire(uint32_t value);

  locks::CriticalSection lock;                          // [interprocess memory access management]
  SharedMemoryPage<TablePageIndexElement> table_index;  // [INDEX]
  std::vector<SharedMemoryPage<ELEMENT_T>*> opened_pages_list;

  const std::string table_name;
  const uint16_t table_max_pages;
  const uint32_t max_elements_in_page;
  const uint32_t record_expire_seconds;
  uint32_t time_to_check_page_expire = 0;

  template <class T>
  friend class SharedMemoryPage;

  template <class T>
  friend class ElementExtractor;
};
}  // namespace shared_mem

// template implementation...
#include "shared_memory.tpp"

#endif