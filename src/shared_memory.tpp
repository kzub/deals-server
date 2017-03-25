#include <cinttypes>
#include <cstring>
#include <iostream>
#include <vector>

#include <errno.h>
#include <fcntl.h> /* For O_* constants */
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "shared_memory.hpp"
#include "statsd_client.hpp"
#include "timing.hpp"

namespace shared_mem {

/*-----------------------------------------------------------------
* TABLE        Constructor
*-----------------------------------------------------------------*/
template <typename ELEMENT_T>
Table<ELEMENT_T>::Table(std::string table_name, uint16_t table_max_pages,
                        uint32_t max_elements_in_page, uint32_t record_expire_seconds)
    : lock{table_name},
      table_index{table_name, table_max_pages},
      table_max_pages(table_max_pages),
      max_elements_in_page(max_elements_in_page),
      record_expire_seconds(record_expire_seconds) {
  std::cout << "Table::Table (" << table_name << ") OK" << std::endl;
}

//-----------------------------------------------------
// Table Destructor
//-----------------------------------------------------
template <typename ELEMENT_T>
Table<ELEMENT_T>::~Table() {
  std::cout << "TABLE (" << table_index.page_name << ") destructor... ";
  release_open_pages();
  std::cout << "OK" << std::endl;
}

//-----------------------------------------------------
// clear_index_record
//-----------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::clear_index_record(TablePageIndexElement& record) {
  record.expire_at = 0;
  record.page_elements_available = max_elements_in_page;
}

//-----------------------------------------------------
// clear_index_record_full
//-----------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::clear_index_record_full(TablePageIndexElement& record) {
  record.expire_at = 0;
  record.page_elements_available = max_elements_in_page;
  std::memset(&record.page_name, 0, MEMPAGE_NAME_MAX_LEN);
}

//-----------------------------------------------------
// processRecords
//-----------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::processRecords(TableProcessor<ELEMENT_T>& processor) {
  //
  release_expired_memory_pages();

  uint32_t timestamp_now = timing::getTimestampSec();
  std::vector<TablePageIndexElement*> records_to_scan;
  records_to_scan.reserve(opened_pages_list.size());  // optimisation

  lock.enter();
  locks::AutoCloser guard(lock);

  for (uint16_t idx = 0; idx < table_max_pages; ++idx) {
    TablePageIndexElement& index_current = table_index.shared_elements[idx];
    // or not used yet (stop here. next pages are unused)
    // [expired][expired][data][expired][data][expired][expired][zero][unused][unused]...[unused]
    //                                                            ^
    if (index_current.expire_at == 0) {
      break;
    }
    // if page not empty and not expired
    // [expired][expired][data][expired][data][expired][expired][zero][unused][unused]...[unused]
    //                     ^              ^
    if (index_current.expire_at > timestamp_now && index_current.expire_at > global_expire_at) {
      records_to_scan.push_back(&index_current);
    }
    // [expired][data][expired][data][expired][expired][expired][zero][unused][unused]...[unused]
    //   ^              ^              ^        ^        ^
    else {
      // skip expired pages
    }
  }

  lock.exit();

  // process every element in every page
  for (const auto record : records_to_scan) {
    const auto page = getPageByName(record->page_name);
    const auto elements = page->getElements();
    const auto size = max_elements_in_page - record->page_elements_available;

    // go throught all elements and apply process function
    for (uint32_t idx = 0; idx < size; ++idx) {
      processor.process_element(elements[idx]);
    }
  }
}

//-----------------------------------------------------
// cleanup
//-----------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::cleanup() {
  uint16_t idx = 0;
  auto index_first = table_index.getElements();
  auto index_current = index_first;

  lock.enter();
  locks::AutoCloser guard(lock);

  for (idx = 0; idx < table_max_pages; ++idx) {
    index_current = index_first + idx;
    // if page not empty and not expired
    if (index_current->expire_at > 0) {
      const auto page = getPageByName(index_current->page_name);
      // mark as deleted
      page->shared_pageinfo->unlinked = true;
      SharedMemoryPage<ELEMENT_T>::unlink(index_current->page_name);
      clear_index_record_full(*index_current);
    } else {
      // stop here. next pages are unused
      break;
    }
  }

  lock.exit();

  release_open_pages();
}

//-----------------------------------------------------
// release_open_pages
//-----------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::release_open_pages() {
  for (auto page : opened_pages_list) {
    delete page;
  }
  opened_pages_list.clear();
}

//-----------------------------------------------------
// Table addRecord
//-----------------------------------------------------
template <typename ELEMENT_T>
ElementExtractor<ELEMENT_T> Table<ELEMENT_T>::addRecord(ELEMENT_T* records_pointer,
                                                        uint32_t records_count,
                                                        uint32_t lifetime_seconds) {
  release_expired_memory_pages();
  checkRecord(records_count);

  uint32_t current_time = timing::getTimestampSec();
  TablePageIndexElement* index_record;
  uint32_t expire_min = UINT32_MAX;
  uint16_t expire_min_idx = 0;
  uint16_t idx;

  auto current_record_type = PageType::UNKNOWN;
  lock.enter();
  locks::AutoCloser guard(lock);
  for (idx = 0; idx < table_max_pages; ++idx) {
    index_record = &table_index.shared_elements[idx];

    if (index_record->expire_at > 0 && expire_min > index_record->expire_at) {
      expire_min = index_record->expire_at;
      expire_min_idx = idx;
    }
    // page expired -> make it empty and use it to save records
    // [expired][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //   ^        ^              ^        ^        ^        ^
    if (index_record->expire_at > 0 && index_record->expire_at < current_time) {
      current_record_type = PageType::EXPIRED;
      break;
    }
    // page exist and fit in size
    // [data][data][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //   ^     ^     ^
    if (index_record->expire_at > 0 && index_record->page_elements_available >= records_count) {
      current_record_type = PageType::CURRENT;
      break;
    }
    // we are at the end of current data
    // [data][data][data][data][data][data][data][data][data][zero][unused][unused]...[unused]
    //                                                         ^     ^- need to be marked as zero
    if (index_record->expire_at == 0) {
      current_record_type = PageType::NEW;
      break;
    }
  }

  if (current_record_type == PageType::NEW) {
    if (shared_mem::isMemLow()) {
      current_record_type = PageType::OLDEST;
      idx = expire_min_idx;
      index_record = &table_index.shared_elements[idx];
      update_global_expire(expire_min);
    }
  }

  std::string insert_page_name = table_index.page_name + ":" + std::to_string(idx);

  if (current_record_type == PageType::NEW && shared_mem::isMemLow()) {
    current_record_type = PageType::OLDEST;
    idx = expire_min_idx;
    index_record = &table_index.shared_elements[idx];
  }

  switch (current_record_type) {
    case PageType::NEW:
      std::memcpy(index_record->page_name, insert_page_name.c_str(), insert_page_name.length());
    case PageType::OLDEST:
    case PageType::EXPIRED:
      clear_index_record(*index_record);
      break;
    case PageType::CURRENT:
      break;
    case PageType::UNKNOWN:
      throw types::Error("addRecord::NO_SPACE_TO_INSERT\n", types::ErrorCode::InternalError);
  }

  uint32_t insert_element_idx = max_elements_in_page - index_record->page_elements_available;
  index_record->page_elements_available -= records_count;
  update_record_expire(index_record, current_time, lifetime_seconds);

  lock.exit();

  if (current_record_type != PageType::CURRENT) {
    reportMemUsage(current_record_type, insert_page_name);
  }
  // we have page and position to insert let's find page in local heap or allocate it
  const auto page = getPageByName(insert_page_name);
  std::memcpy(&page->shared_elements[insert_element_idx], records_pointer,
              sizeof(ELEMENT_T) * records_count);

  return ElementExtractor<ELEMENT_T>{*this, insert_page_name, insert_element_idx, records_count};
}

//-----------------------------------------------------
// checkRecord
//-----------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::checkRecord(uint32_t& records_count) {
  if (records_count > max_elements_in_page) {
    throw types::Error(
        "checkRecord::RECORD_SIZE_TO_BIG size:" + std::to_string(records_count) + "\n",
        types::ErrorCode::InternalError);
  }
}

//-----------------------------------------------------
// update_record_expire
//-----------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::update_record_expire(TablePageIndexElement* index_record,
                                            uint32_t current_time, uint32_t lifetime_seconds) {
  uint32_t expire_time;
  if (lifetime_seconds != 0) {
    expire_time = current_time + lifetime_seconds;
  } else {
    expire_time = current_time + record_expire_seconds;
  }

  // update page expire time only if record expire time greater
  if (expire_time > index_record->expire_at) {
    index_record->expire_at = expire_time;
  }
}

//-----------------------------------------------------
// localGetPageByName
//-----------------------------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>* Table<ELEMENT_T>::localGetPageByName(const std::string& page_to_look) {
  for (const auto& page : opened_pages_list) {
    if (page->page_name == page_to_look) {
      return page;
    }
  }
  return nullptr;
}

//-----------------------------------------------------
// getPagesByName
//-----------------------------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>* Table<ELEMENT_T>::getPageByName(const std::string& page_to_look) {
  auto page = localGetPageByName(page_to_look);

  if (page == nullptr) {
    page = new SharedMemoryPage<ELEMENT_T>{page_to_look, max_elements_in_page};
    opened_pages_list.push_back(page);
  }

  return page;
}

//------------------------------------------------------------------
// release_expired_memory_pages | auto release Table expired memory
//------------------------------------------------------------------
// [a][ab][b][c][d]      Table A
// [aaa][bb][cc][cddd]   Table B
//      ^ remove at this point will release [aaa] but not release [ab]
//        in this case [ab] will point to unexisted page
//        application should care about Table A & Table B consistency
template <typename ELEMENT_T>
void Table<ELEMENT_T>::release_expired_memory_pages() {
  uint32_t current_time = timing::getTimestampSec();

  // check local timer
  if (time_to_check_page_expire > current_time) {
    return;
  }
  time_to_check_page_expire = current_time + MEMPAGE_CHECK_EXPIRED_PAGES_INTERVAL_SEC;

  lock.enter();
  locks::AutoCloser guard(lock);

  // check shared timer, only one process should perform maintenance
  if (table_index.shared_pageinfo->expiration_check <= current_time) {
    uint16_t idx = 0;
    uint16_t last_data_idx = 0;

    // update shared data
    table_index.shared_pageinfo->expiration_check = time_to_check_page_expire;

    // search for free space in all table pages
    for (idx = 0; idx < table_max_pages; ++idx) {
      // current page row (pointer to shared memory)
      const TablePageIndexElement& index_record = table_index.shared_elements[idx];

      // page expired -> make it empty and use it to save records
      // [expired][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
      //                     ^
      if (index_record.expire_at > 0 &&
          index_record.expire_at + MEMPAGE_REMOVE_EXPIRED_PAGES_DELAY_SEC > current_time &&
          index_record.expire_at + MEMPAGE_REMOVE_EXPIRED_PAGES_DELAY_SEC > global_expire_at) {
        //                         ^^^ to be sure page not being used by anyone
        last_data_idx = idx;
      }

      // [expired][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
      //                                                               ^
      if (index_record.expire_at == 0) {
        break;
      }
    }

    uint16_t cleared_counter = 0;
    // mark as unlinked, release this pages locally and unlink from ramfs
    // [expired][expired][data][expired][data][expired][expired][zero][unused][unused]...[unused]
    //                   last_data_idx ---^     ^        ^        ^--- idx
    if (idx > 0 && last_data_idx < --idx) {
      for (; last_data_idx < idx; idx--) {
        auto& index_record = table_index.shared_elements[idx];
        const auto page = getPageByName(index_record.page_name);
        page->shared_pageinfo->unlinked = true;
        SharedMemoryPage<ELEMENT_T>::unlink(page->page_name);

        clear_index_record_full(index_record);
        // clear only certain portion per time;
        if (MEMPAGE_REMOVE_EXPIRED_PAGES_AT_ONCE <= ++cleared_counter) {
          break;
        }
      }
    }
  }

  lock.exit();

  // clear opened_pages_list from unlinked items, all processes must do that
  std::vector<SharedMemoryPage<ELEMENT_T>*> new_pages_list;
  for (auto page : opened_pages_list) {
    if (page->shared_pageinfo->unlinked) {
      std::cout << "RELEASING unlinked page:" << page->page_name << std::endl;
      delete page;
    } else {
      new_pages_list.push_back(page);
    }
  }
  opened_pages_list = std::move(new_pages_list);
}

/*-----------------------------------------------------------------
* SHARED MEMORY
*-----------------------------------------------------------------*/

//------------------------------------------------------------
// SharedMemoryPage Constructor
//------------------------------------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>::SharedMemoryPage(std::string page_name, uint32_t elements)
    : page_name(page_name), shared_memory(nullptr) {
  //
  if (page_name.length() == 0) {
    std::cout << "ERROR SharedMemoryPage::SharedMemoryPage EMPTY_PAGE_NAME" << std::endl;
    throw types::Error("EMPTY_PAGE_NAME\n", types::ErrorCode::InternalError);
  }

  // max 6 digits (uint16_t) ->  ':65536' - suffix for pages
  if (page_name.length() > MEMPAGE_NAME_MAX_LEN - 6) {
    std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage LONG_PAGE_NAME:" << page_name
              << " (max:" << MEMPAGE_NAME_MAX_LEN - 6 << ")" << std::endl;
    throw types::Error("LONG_PAGE_NAME\n", types::ErrorCode::InternalError);
  }

  // two processes could try to open the same page in a same time
  // first will create page, but not truncate yet, second will open it
  // compare size and remove page because it has wrong(zero) size
  locks::CriticalSection cslock(page_name);
  cslock.enter();  // <= will auto exited on class destruction

  bool new_memory_allocated = false;
  page_memory_size = sizeof(Page_information) + sizeof(ELEMENT_T) * elements;
  uint32_t aligned_pages = page_memory_size / sysconf(_SC_PAGE_SIZE);
  page_memory_size = (aligned_pages + 1) * sysconf(_SC_PAGE_SIZE);

  // try to create page
  int fd = shm_open(page_name.c_str(), O_RDWR | O_CREAT | O_EXCL, (mode_t)0666);
  if (fd == -1) {
    // already exist. try to open
    if (errno == EEXIST) {
      fd = shm_open(page_name.c_str(), O_RDWR | O_CREAT, (mode_t)0666);
    }

    if (fd == -1) {
      std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage Cannot create or open memory page:"
                << page_name << " errno:" << errno << std::endl;
      throw types::Error("CANNOT_OPEN_SHMEM_PAGE\n", types::ErrorCode::InternalError);
    }

    std::cout << "OPENED:" << page_name << std::endl;
  } else {
    if (!shared_mem::isMemAvailable()) {
      std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage LOW SHARED MEMORY" << std::endl;
      close(fd);  // dont remove file!!! shm_unlink(page_name.c_str());
      throw types::Error("ERR_LOW_SHMEM\n", types::ErrorCode::InternalError);
    }
    // setup page size
    int res = ftruncate(fd, page_memory_size);
    if (res == -1) {
      std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage cant truncate:" << errno << " "
                << page_name << " REMOVING..." << std::endl;
      shm_unlink(page_name.c_str());
      close(fd);
      throw types::Error("ERR_CREATE_SHMEM_PAGE\n", types::ErrorCode::InternalError);
    }
    new_memory_allocated = true;
    std::cout << "CREATE:" << page_name << " size:" << page_memory_size << std::endl;
  }

  // read page size
  struct stat buf;
  fstat(fd, &buf);
  int size = buf.st_size;

  if (size != page_memory_size) {
    std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage size != page_memory_size (" << page_name
              << ") " << size << " != " << page_memory_size << " REMOVING..." << std::endl;
    shm_unlink(page_name.c_str());
    close(fd);
    throw types::Error("ERR_WRONG_SHMEM_PAGE_SIZE\n", types::ErrorCode::InternalError);
  }

  // map page to process local memory
  void* map = mmap(nullptr, page_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  // After a call to mmap(2) the file descriptor may be closed without affecting
  // the memory mapping. http://linux.die.net/man/3/shm_open
  close(fd);

  if (map == MAP_FAILED) {
    std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage MAP_FAILED:" << errno << " page_name("
              << page_name << ") size:" << page_memory_size << std::endl;
    // no shm_unlink(page_name.c_str());
    throw types::Error("ERR_SHMEM_MAP_FAILED\n", types::ErrorCode::InternalError);
  }

  shared_memory = map;
  shared_pageinfo = (Page_information*)shared_memory;
  shared_elements = (ELEMENT_T*)((uint8_t*)shared_memory + sizeof(Page_information));

  if (new_memory_allocated) {
    memset(shared_memory, 0, page_memory_size);
    shared_pageinfo->unlinked = false;
    shared_pageinfo->expiration_check = 0;
  }
};

//------------------------------------------------------------
// SharedMemoryPage Destructor
//------------------------------------------------------------
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
    shared_memory = nullptr;
  }
}

//------------------------------------------------------------
// getElements
//------------------------------------------------------------
template <typename ELEMENT_T>
ELEMENT_T* SharedMemoryPage<ELEMENT_T>::getElements() {
  return shared_elements;
}

//------------------------------------------------------------
// operator ELEMENT_T*()
//------------------------------------------------------------
template <typename ELEMENT_T>
ElementExtractor<ELEMENT_T>::operator ELEMENT_T*() {
  return get_element_data();
}

/*-----------------------------------------------------------------
* ElementExtractor get_element_data
*-----------------------------------------------------------------*/
template <typename ELEMENT_T>
ELEMENT_T* ElementExtractor<ELEMENT_T>::get_element_data() {
  const auto page = table.getPageByName(page_name);
  return page->getElements() + index;
}
}  // namespace shared_mem
