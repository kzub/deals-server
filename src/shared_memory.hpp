#ifndef SRC_SHAREDMEM_HPP
#define SRC_SHAREDMEM_HPP

#include <sys/mman.h>
#include <cinttypes>
#include <iostream>
#include <vector>

#include "locks.hpp"

namespace shared_mem {

#define MEMPAGE_NAME_MAX_LEN 20

enum ErrorCodes {
  NO_ERROR = 0,
  RECORD_SIZE_TO_BIG = 1,
  NO_SPACE_TO_INSERT = 2,
  CANT_FIND_PAGE = 3
};

template <typename ELEMENT_T>
class SharedMemoryPage;
template <typename ELEMENT_T>
class Table;

// information about all open pages in all processes
struct TablePageIndexElement {
  uint32_t expire_at;
  uint32_t page_elements_available;
  char page_name[MEMPAGE_NAME_MAX_LEN];
};

template <typename ELEMENT_T>
class ElementPointer {
 public:
  ElementPointer(Table<ELEMENT_T>& table, ErrorCodes error)
      : error(error), page_name(""), index(0), size(0), table(table){};
  ElementPointer(Table<ELEMENT_T>& table, std::string page_name, uint32_t index, uint32_t size)
      : error(NO_ERROR), page_name(page_name), index(index), size(size), table(table){};

  ELEMENT_T* get_data();

  const ErrorCodes error;
  const std::string page_name;
  const uint32_t index;
  const uint32_t size;

 private:
  Table<ELEMENT_T>& table;
};

template <typename ELEMENT_T>
class TableProcessor {
 protected:
  // called before iteration process
  virtual void pre_process_function(){};
  /* function that will be called for iterating over all not expired pages in
   * table */
  virtual bool process_function(ELEMENT_T* elements, uint32_t size);
  // called after iteration process
  virtual void post_process_function(){};

  template <class T>
  friend class Table;
};

template <typename ELEMENT_T>
class Table {
 public:
  Table(std::string table_name, uint16_t table_max_pages, uint32_t max_elements_in_page,
        uint32_t record_expire_seconds);
  // cleanup all shared memory mappings on exit
  ~Table();

  ElementPointer<ELEMENT_T> addRecord(ELEMENT_T* el, uint32_t size = 1,
                                      uint32_t lifetime_seconds = 0);
  void process(TableProcessor<ELEMENT_T>* result);
  void cleanup();

 private:
  SharedMemoryPage<ELEMENT_T>* localGetPageByName(std::string page_name_to_look);
  SharedMemoryPage<ELEMENT_T>* getPageByName(std::string page_name_to_look);

  locks::CriticalSection* lock;
  uint16_t table_max_pages;
  uint16_t last_known_index_length;
  uint32_t max_elements_in_page;
  std::vector<SharedMemoryPage<ELEMENT_T>*> opened_pages_list;
  SharedMemoryPage<TablePageIndexElement>* table_index;
  uint32_t record_expire_seconds;

  template <class T>
  friend class SharedMemoryPage;

  template <class T>
  friend class ElementPointer;
};

template <typename ELEMENT_T>
class SharedMemoryPage {
 public:
  bool isAllocated();
  ELEMENT_T* getElements();
  ~SharedMemoryPage();

 private:
  struct page_information {
    bool unlinked;
  };

  SharedMemoryPage(std::string page_name, uint32_t elements);

  std::string page_name;
  uint32_t page_memory_size;

  // look to shared memeory
  void* shared_memory;
  ELEMENT_T* shared_elements;
  page_information* shared_pageinfo;

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
};
}

// template implementation...
#include "shared_memory.tpp"

#endif