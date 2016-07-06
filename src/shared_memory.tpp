#include <cinttypes>
#include <cstring>
#include <iostream>
#include <vector>

#include <errno.h>
#include <fcntl.h> /* For O_* constants */
#include <string.h>
#include <unistd.h>

#include "shared_memory.hpp"
#include "timing.hpp"

namespace shared_mem {
#include <sys/mman.h>

/*-----------------------------------------------------------------
* TABLE
*-----------------------------------------------------------------*/
// Table Constructor ----------------------------------------------
template <typename ELEMENT_T>
Table<ELEMENT_T>::Table(std::string table_name, uint16_t table_max_pages,
                        uint32_t max_elements_in_page, uint32_t record_expire_seconds)
    : table_max_pages(table_max_pages),
      last_known_index_length(0),
      max_elements_in_page(max_elements_in_page),
      record_expire_seconds(record_expire_seconds) {
  // max 6 digits (uint16_t) ->  ':65536' - suffix for pages
  if (table_name.length() > MEMPAGE_NAME_MAX_LEN - 6) {
    std::cerr << "ERROR Table::Table TABLE_NAME_TOO_LONG" << table_name
              << "(max:" << MEMPAGE_NAME_MAX_LEN - 6 << ")" << std::endl;
    throw "TABLE_NAME_TOO_LONG";
  }

  // open existed index or make new one
  table_index = new SharedMemoryPage<TablePageIndexElement>(table_name, table_max_pages);
  if (!table_index->isAllocated()) {
    std::cerr << "ERROR Table::Table CANNOT_ALLOCATE_TABLE_INDEX for: " << table_name
              << " pages:" << table_max_pages << std::endl;
    throw "CANNOT_ALLOCATE_TABLE_INDEX";
  }

  clear_index_record(table_index->shared_elements[0]);
  lock = new locks::CriticalSection(table_name);
  std::cout << "Table::Table (" << table_name << ") OK" << std::endl;
}

// Table Destructor ----------------------------------------------
template <typename ELEMENT_T>
Table<ELEMENT_T>::~Table() {
  std::cout << "TABLE (" << table_index->page_name << ") destructor... ";
  // cleanup all shared memory mappings on exit
  release_open_pages();

  // delete index
  delete table_index;
  delete lock;
  std::cout << "OK" << std::endl;
}

// processTablePages ----------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::clear_index_record(TablePageIndexElement& record) {
  record.expire_at = 0;
  record.page_elements_available = max_elements_in_page;
  record.page_name[0] = 0;
}

// processTablePages ----------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::processRecords(TableProcessor<ELEMENT_T>& processor) {
  // check if there is time to release some pages
  release_expired_memory_pages();

  // *** Make a copy to local heap ***
  lock->enter();

  std::vector<TablePageIndexElement> records_to_scan;

  uint16_t idx = 0;
  // uint16_t last_not_expired_idx = 0;
  uint32_t timestamp_now = timing::getTimestampSec();

  TablePageIndexElement* index_first = table_index->getElements();
  TablePageIndexElement* index_current;

  // search for pages to scan (not expired)
  for (idx = 0; idx < table_max_pages; idx++) {
    // current page row (pointer to shared memory)
    index_current = index_first + idx;

    // if page not empty and not expired
    if (index_current->expire_at >= timestamp_now) {
      // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
      //                     ^              ^
      records_to_scan.push_back(*index_current);
      // last_not_expired_idx = idx;
    }
    // or not used yet
    else if (index_current->expire_at == 0) {
      // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
      //                                                                              ^
      // stop here. next pages are unused
      break;
    } else {
      // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
      //   ^        ^              ^              ^        ^        ^        ^
      // or if page expired add it to list
      // unlinked_records.push_back(*index_current);
    }
  }

  lock->exit();

  bool continue_iteration;
  for (auto record : records_to_scan) {
    // call table processor routine
    SharedMemoryPage<ELEMENT_T>* page = getPageByName(record.page_name);
    if (page == nullptr) {
      std::cerr << "ERROR Table::processRecords Cannot allocate page Table::processRecords()"
                << std::endl;
      continue;
    }

    continue_iteration = processor.process_function(
        page->getElements(), max_elements_in_page - record.page_elements_available);

    if (continue_iteration == false) {
      break;
    }
  }
}

// cleanup ----------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::cleanup() {
  lock->enter();
  uint16_t idx = 0;
  TablePageIndexElement* index_first = table_index->getElements();
  TablePageIndexElement* index_current;

  // search for free space in pages
  for (idx = 0; idx < table_max_pages; idx++) {
    // current page row (pointer to shared memory)
    index_current = index_first + idx;
    // if page not empty and not expired

    if (index_current->expire_at > 0) {
      // mark as deleted
      SharedMemoryPage<ELEMENT_T>* page = getPageByName(index_current->page_name);
      if (page == nullptr) {
        std::cerr << "ERROR Table::cleanup Cannot allocate page Table::cleanup()" << std::endl;
        continue;
      }
      page->shared_pageinfo->unlinked = true;
      SharedMemoryPage<ELEMENT_T>::unlink(index_current->page_name);
      clear_index_record(*index_current);
    } else {
      // stop here. next pages are unused
      break;
    }
  }

  lock->exit();

  release_open_pages();
  SharedMemoryPage<ELEMENT_T>::unlink(table_index->page_name);
}

// release_open_pages ----------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::release_open_pages() {
  for (auto page : opened_pages_list) {
    delete page;
  }
  opened_pages_list.clear();
}

// Table addRecord ----------------------------------------------
template <typename ELEMENT_T>
ElementPointer<ELEMENT_T> Table<ELEMENT_T>::addRecord(ELEMENT_T* records_pointer,
                                                      uint32_t records_cout,
                                                      uint32_t lifetime_seconds) {
  // check if there is time to release some pages
  release_expired_memory_pages();

  if (records_cout > max_elements_in_page) {
    std::cout << "ERROR Table::addRecord records_cout > max_elements_in_page:"
              << " records_cout:" << records_cout
              << " max_elements_in_page:" << max_elements_in_page << std::endl;

    return ElementPointer<ELEMENT_T>(*this, ErrorCode::RECORD_SIZE_TO_BIG);
  }

  std::string insert_page_name;
  uint32_t insert_element_idx;
  uint32_t current_time = timing::getTimestampSec();
  uint16_t idx = 0;
  TablePageIndexElement* index_record;
  bool current_record_was_cleared;

  // std::cout << "CURRENT_TIME: " << current_time << std::endl;
  lock->enter();

  // search for free space in pages
  for (idx = 0; idx < table_max_pages; idx++) {
    // current page row (pointer to shared memory)
    index_record = &table_index->shared_elements[idx];
    // std::cout << "===> " << table_index->page_name + ":" +
    // std::to_string(idx) << " AVAIL:" << index_record->page_elements_available
    // << " expire_at:" << std::to_string(index_record->expire_at) << std::endl;

    current_record_was_cleared = false;
    // page expired -> make it empty and use it to save records
    // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //   ^        ^              ^              ^        ^        ^        ^
    if (index_record->expire_at > 0 && index_record->expire_at < current_time) {
      index_record->expire_at = 0;
      clear_index_record(*index_record);
      current_record_was_cleared = true;
    }

    // page exist and not fit (go next)
    // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //   ^        ^              ^              ^        ^        ^        ^
    else if (index_record->expire_at > 0 && index_record->page_elements_available < records_cout) {
      continue;
    }

    // page fit our needs. let's use it
    insert_page_name = table_index->page_name + ":" + std::to_string(idx);

    // page is empty -> use it
    // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //                                                                              ^
    if (index_record->expire_at == 0) {
      // page still has full capacity, lets insert at the begining
      insert_element_idx = 0;
      if (current_record_was_cleared) {
        std::cout << "USE EXPIRED page:" << insert_page_name << std::endl;
      } else {
        std::cout << "USE NEW page:" << insert_page_name << std::endl;
      }

      // calculate capacity after we will put records
      index_record->page_elements_available = max_elements_in_page - records_cout;
      // copy page_name to shared meme
      std::memcpy(index_record->page_name, insert_page_name.c_str(), insert_page_name.length());

      // fill with zero next index row in case last row  has no space
      // [data][data][data][data][data][data][data][data][data][zero][unused][unused]...[unused]
      //                                                         ^     ^- need to be marked as zero
      if (!current_record_was_cleared && (idx + 1) < table_max_pages) {
        // table_index->shared_elements[idx + 1].expire_at = 0;
        clear_index_record(table_index->shared_elements[idx + 1]);
      }
    }
    // or use current page
    else {
      // std::cout << "USE EXISTING page:" << insert_page_name << std::endl;
      // max_elements_in_page stored in table
      // must be the same on all program instances
      insert_element_idx = max_elements_in_page - index_record->page_elements_available;
      // decrease available elements
      index_record->page_elements_available -= records_cout;
    }

    // page will expire after N seconds
    uint32_t expire_time;
    if (lifetime_seconds != 0) {
      expire_time = current_time + lifetime_seconds;
    } else {
      expire_time = current_time + record_expire_seconds;
    }

    // update only if time greater
    if (expire_time > index_record->expire_at) {
      index_record->expire_at = expire_time;
    }

    break;
  }

  lock->exit();

  if (insert_page_name.length() == 0) {
    std::cerr << "ERROR Table::addRecord() insert_page_name.length() == 0" << std::endl;
    return ElementPointer<ELEMENT_T>(*this, ErrorCode::NO_SPACE_TO_INSERT);
  }

  // now we have page to insert
  // and position to insert
  // let's look for page now in local heap or allocate it
  SharedMemoryPage<ELEMENT_T>* page = getPageByName(insert_page_name);

  if (page == nullptr) {
    std::cerr << "ERROR Table::addRecord() page == nullptr" << std::endl;
    return ElementPointer<ELEMENT_T>(*this, ErrorCode::CANT_FIND_PAGE);
  }

  // copy array of (records_cout) elements to shared memeory
  std::memcpy(&page->shared_elements[insert_element_idx], records_pointer,
              sizeof(ELEMENT_T) * records_cout);

  // std::cout << "COPY :" << insert_page_name << " idx:" << insert_element_idx
  // << " cout:" << records_cout <<	" size:" << sizeof(ELEMENT_T)*records_cout << std::endl;
  return ElementPointer<ELEMENT_T>(*this, insert_page_name, insert_element_idx, records_cout);
}

// Table localGetPageByName -------------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>* Table<ELEMENT_T>::localGetPageByName(std::string page_name_to_look) {
  // find page in open pages list
  for (auto page : opened_pages_list) {
    if (page->page_name == page_name_to_look) {
      return page;
    }
  }

  return nullptr;
}

// Table getPageByName -------------------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>* Table<ELEMENT_T>::getPageByName(std::string page_name_to_look) {
  // let's look for page now in local heap
  SharedMemoryPage<ELEMENT_T>* page = localGetPageByName(page_name_to_look);

  // if not already open or created -> do it
  if (page == nullptr || !page->isAllocated()) {
    page = new SharedMemoryPage<ELEMENT_T>(page_name_to_look, max_elements_in_page);

    if (!page->isAllocated()) {
      std::cerr << "ERROR SharedMemoryPage::getPageByName page not allocated" << std::endl;
      delete page;
      return nullptr;
    }

    // close handlers when destroyed
    opened_pages_list.push_back(page);
  }

  return page;
}

//------------------------------------------------------------
// auto release Table expired memeory
//------------------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::release_expired_memory_pages() {
  // MEMPAGE_CHECK_EXPIRED_PAGES_INTERVAL_SEC
  // uint16_t clear_limit = MEMPAGE_REMOVE_EXPIRED_PAGES_AT_ONCE;
  uint32_t current_time = timing::getTimestampSec();
  uint16_t idx = 0;
  uint16_t last_data_idx = 0;

  if (time_to_check_page_expire > current_time) {
    // std::cout << "to ealry (release_expired_memory_pages)" << std::endl;
    return;
  }
  time_to_check_page_expire = current_time + MEMPAGE_CHECK_EXPIRED_PAGES_INTERVAL_SEC;

  lock->enter();

  // search for free space in pages
  for (idx = 0; idx < table_max_pages; idx++) {
    // current page row (pointer to shared memory)
    const TablePageIndexElement& index_record = table_index->shared_elements[idx];

    // page expired -> make it empty and use it to save records
    // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //                     ^              ^
    if (index_record.expire_at > 0 && index_record.expire_at >= current_time) {
      last_data_idx = idx;
    }

    // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //                                                                              ^
    if (index_record.expire_at == 0) {
      break;
    }
  }

  uint16_t cleared_counter = 0;
  // mark as unlinked, release this pages locally and unlink
  // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
  //                   last_data_idx ---^     ^        ^        ^        ^        ^--- idx
  if (idx > 0 && last_data_idx < --idx) {
    for (; last_data_idx < idx; idx--) {
      TablePageIndexElement& index_record = table_index->shared_elements[idx];
      SharedMemoryPage<ELEMENT_T>* page = getPageByName(index_record.page_name);

      if (page == nullptr) {
        std::cerr << "ERROR Table::release_expired_memory_pages cannot acquire page:"
                  << index_record.page_name << std::endl;
        continue;
      }

      // std::cout << "CLEARING page memory:" << index_record.page_name << std::endl;
      page->shared_pageinfo->unlinked = true;
      SharedMemoryPage<ELEMENT_T>::unlink(page->page_name);

      clear_index_record(index_record);
      // clear only certain portion per time;
      if (MEMPAGE_REMOVE_EXPIRED_PAGES_AT_ONCE <= ++cleared_counter) {
        break;
      }
    }
  }

  lock->exit();

  // clear opened_pages_list from unlinked items
  std::vector<SharedMemoryPage<ELEMENT_T>*> new_pages_list;
  for (auto page : opened_pages_list) {
    if (page->shared_pageinfo->unlinked) {
      // std::cout << "RELEASING local page:" << page->page_name << std::endl;
      delete page;
    } else {
      new_pages_list.push_back(page);
    }
  }
  opened_pages_list = new_pages_list;
}

/*-----------------------------------------------------------------
* SHARED MEMORY
*-----------------------------------------------------------------*/

// SharedMemoryPage Constructor ----------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>::SharedMemoryPage(std::string page_name, uint32_t elements)
    : page_name(page_name), shared_memory(nullptr) {
  if (!page_name.length()) {
    std::cout << "ERROR SharedMemoryPage::SharedMemoryPage page_name empty" << std::endl;
    return;
  }

  page_memory_size = sizeof(page_information) + sizeof(ELEMENT_T) * elements;

  // try to create page
  bool new_memory_allocated = false;
  int fd = shm_open(page_name.c_str(), O_RDWR | O_CREAT | O_EXCL, (mode_t)0666);
  if (fd == -1) {
    // try to open if page already exists
    if (errno == EEXIST) {
      fd = shm_open(page_name.c_str(), O_RDWR | O_CREAT, (mode_t)0666);
    }

    if (fd == -1) {
      std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage Cannot create or open memory page:"
                << errno << " " << page_name << std::endl;
      return;
    }

    std::cout << "OPENED:" << page_name << std::endl;
  } else {
    // if new setup page size
    int res = ftruncate(fd, page_memory_size);
    if (res == -1) {
      std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage cant truncate:" << errno << " "
                << page_name << std::endl;
      close(fd);
      return;
    }
    new_memory_allocated = true;
    std::cout << "CREATE:" << page_name << " size:" << page_memory_size << std::endl;
  }

  void* map = mmap(nullptr, page_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  // After a call to mmap(2) the file descriptor may be closed without affecting
  // the memory mapping.
  // http://linux.die.net/man/3/shm_open
  close(fd);

  if (map == MAP_FAILED) {
    std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage MAP_FAILED:" << errno << " page_name("
              << page_name << ") size:" << page_memory_size << " REMOVING..." << std::endl;
    shm_unlink(page_name.c_str());
    return;
  }

  shared_memory = map;
  shared_pageinfo = (page_information*)shared_memory;
  shared_elements = (ELEMENT_T*)((uint8_t*)shared_memory + sizeof(page_information));

  if (new_memory_allocated) {
    // cleanup info structure & first element
    memset(shared_memory, 0, sizeof(page_information) + sizeof(ELEMENT_T));
    shared_pageinfo->unlinked = false;
    // std::cout << "MEMSET:" << page_name << " bytes:" << sizeof(page_information) +
    // sizeof(ELEMENT_T) << std::endl;
  }
  // std::cout << "MAKE PAGE: " << page_name <<  "(" << page_memory_size << ") " << std::endl;
};

// SharedMemoryPage Destructor -----------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>::~SharedMemoryPage() {
  // The munmap() system call deletes the mappings for the specified address
  // range, and causes further references to addresses within the range to
  // generate invalid memory references. The region is also automatically
  // unmapped when the process is terminated. On the other hand, closing the
  // file descriptor does not unmap the region.
  if (shared_memory != nullptr) {
    std::cout << "FREE " << page_name << " (" << page_memory_size << ") ";
    int res_unmap = munmap(shared_memory, page_memory_size);
    std::cout << (res_unmap == 0 ? "OK " : "FAIL ") << std::endl;
  }
}

// getElements() -------------------------------------------------
template <typename ELEMENT_T>
ELEMENT_T* SharedMemoryPage<ELEMENT_T>::getElements() {
  return shared_elements;
}

template <typename ELEMENT_T>
bool SharedMemoryPage<ELEMENT_T>::isAllocated() {
  return shared_memory != nullptr;
}

/*-----------------------------------------------------------------
* ElementPointer get_data
*-----------------------------------------------------------------*/
template <typename ELEMENT_T>
ELEMENT_T* ElementPointer<ELEMENT_T>::get_data() {
  if (error != ErrorCode::NO_ERROR) {
    return nullptr;
  }

  SharedMemoryPage<ELEMENT_T>* page = table.getPageByName(page_name);
  if (page == nullptr) {
    std::cerr << "ERROR ElementPointer::get_data" << std::endl;
    return nullptr;
  }

  return page->getElements() + index;
}
}
