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

  lock = new locks::CriticalSection(table_name);
  std::cout << "Table::Table (" << table_name << ") OK" << std::endl;
}

//-----------------------------------------------------
// Table Destructor
//-----------------------------------------------------
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

//-----------------------------------------------------
// clear_index_record
//-----------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::clear_index_record(TablePageIndexElement& record) {
  record.expire_at = 0;
  record.page_elements_available = max_elements_in_page;
  record.page_name[0] = 0;
}

//-----------------------------------------------------
// processRecords
//-----------------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::processRecords(TableProcessor<ELEMENT_T>& processor) {
  // check if there is time to release some pages
  release_expired_memory_pages();

  // *** Make a copy to local heap ***
  lock->enter();

  std::vector<TablePageIndexElement> records_to_scan;
  records_to_scan.reserve(opened_pages_list.size());  // optimisation

  uint32_t timestamp_now = timing::getTimestampSec();

  TablePageIndexElement* index_first = table_index->getElements();
  TablePageIndexElement* index_current;

  // search for pages to scan (not expired)
  for (uint16_t idx = 0; idx < table_max_pages; ++idx) {
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
    }
  }

  lock->exit();

  // process every element in every page
  for (const auto& record : records_to_scan) {
    // call table processor routine
    auto* page = getPageByName(record.page_name);
    if (page == nullptr) {
      std::cerr << "ERROR Table::processRecords Cannot allocate page Table::processRecords()"
                << std::endl;
      continue;
    }

    if (max_elements_in_page == record.page_elements_available) {
      continue;
    }

    const auto elements = page->getElements();
    const auto size = max_elements_in_page - record.page_elements_available;

    // processor.type == element_processor
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
  lock->enter();
  uint16_t idx = 0;
  TablePageIndexElement* index_first = table_index->getElements();
  TablePageIndexElement* index_current;

  // search for free space in pages
  for (idx = 0; idx < table_max_pages; ++idx) {
    // current page row (pointer to shared memory)
    index_current = index_first + idx;
    // if page not empty and not expired

    if (index_current->expire_at > 0) {
      // mark as deleted
      auto* page = getPageByName(index_current->page_name);
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
  TablePageIndexElement* index_record;
  bool current_record_was_cleared;

  // std::cout << "CURRENT_TIME: " << current_time << std::endl;
  lock->enter();

  // search for free space in pages
  for (uint16_t idx = 0; idx < table_max_pages; ++idx) {
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
      // index_record->expire_at = 0;
      clear_index_record(*index_record);
      current_record_was_cleared = true;
    }

    // page exist and not fit (go next)
    // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //   ^        ^              ^              ^        ^        ^        ^
    else if (index_record->expire_at > 0 && index_record->page_elements_available < records_cout) {
      continue;
    }

    // we need to allocate new page and we have low memory
    if (index_record->expire_at == 0 && isLowMem()) {
      // instead allocating new page overwrite existing one. an oldest one.
      idx = get_oldest_idx();
      index_record = &table_index->shared_elements[idx];
      index_record->expire_at = 1;
      index_record->page_elements_available = max_elements_in_page;
      insert_page_name = index_record->page_name;
      std::cout << "USE OLDEST page:" << insert_page_name << std::endl;
      statsd::metric.inc("dealsrv.page_use", {{"page", "oldest"}});
    } else {
      // page fit our needs. let's use it
      insert_page_name = table_index->page_name + ":" + std::to_string(idx);
    }

    // page is empty -> use it
    // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //                                                                              ^
    if (index_record->expire_at == 0) {
      // page still has full capacity, lets insert at the begining
      insert_element_idx = 0;
      if (current_record_was_cleared) {
        std::cout << "USE EXPIRED page:" << insert_page_name << std::endl;
        statsd::metric.inc("dealsrv.page_use", {{"page", "expired"}});
      } else {
        std::cout << "USE NEW page:" << insert_page_name << std::endl;
        statsd::metric.inc("dealsrv.page_use", {{"page", "new"}});
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

    // update page expire time only if record expire time greater
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
  auto* page = getPageByName(insert_page_name);

  if (page == nullptr) {
    std::cerr << "ERROR Table::addRecord() page == nullptr" << std::endl;
    return ElementPointer<ELEMENT_T>(*this, ErrorCode::CANT_FIND_PAGE);
  }

  // copy array of (records_cout) elements to shared memeory
  std::memcpy(&page->shared_elements[insert_element_idx], records_pointer,
              sizeof(ELEMENT_T) * records_cout);

  // std::cout << "COPY :" << insert_page_name << " idx:" << insert_element_idx
  // << " cout:" << records_cout << " size:" << sizeof(ELEMENT_T)*records_cout << std::endl;
  return ElementPointer<ELEMENT_T>(*this, insert_page_name, insert_element_idx, records_cout);
}

//-----------------------------------------------------
// get_oldest_idx
//-----------------------------------------------------
template <typename ELEMENT_T>
uint16_t Table<ELEMENT_T>::get_oldest_idx() {
  if (!lock->is_locked()) {
    std::cerr << "ERROR NOT_LOCKED_WHILE_FIND_OLDEST" << std::endl;
    return 0;
  }

  uint32_t min_expire = UINT32_MAX;
  uint16_t min_idx = 0;

  // search for free space in pages
  for (uint16_t idx = 0; idx < table_max_pages; ++idx) {
    // current page row (pointer to shared memory)
    auto index_record = &table_index->shared_elements[idx];

    if (index_record->expire_at == 0) {
      break;
    }

    if (min_expire > index_record->expire_at) {
      min_expire = index_record->expire_at;
      min_idx = idx;
    }
  }

  return min_idx;
}

//-----------------------------------------------------
// localGetPageByName
//-----------------------------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>* Table<ELEMENT_T>::localGetPageByName(const std::string& page_to_look) {
  // find page in open pages list
  for (auto& page : opened_pages_list) {
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
  // let's look for page now in local heap
  auto* page = localGetPageByName(page_to_look);

  // if not already open or created -> do it
  if (page == nullptr || !page->isAllocated()) {
    page = new SharedMemoryPage<ELEMENT_T>(page_to_look, max_elements_in_page);

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
  uint16_t cleared_counter = 0;

  // check local timer
  if (time_to_check_page_expire > current_time) {
    return;
  }
  time_to_check_page_expire = current_time + MEMPAGE_CHECK_EXPIRED_PAGES_INTERVAL_SEC;

  lock->enter();
  // check shared timer
  // only one process should perform maintenance
  if (table_index->shared_pageinfo->expiration_check <= current_time) {
    uint16_t idx = 0;
    uint16_t last_data_idx = 0;

    // update shared data
    table_index->shared_pageinfo->expiration_check = time_to_check_page_expire;

    // search for free space in all table pages
    for (idx = 0; idx < table_max_pages; ++idx) {
      // current page row (pointer to shared memory)
      const auto& index_record = table_index->shared_elements[idx];

      // page expired -> make it empty and use it to save records
      // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
      //                     ^              ^
      if (index_record.expire_at > 0 &&
          index_record.expire_at + MEMPAGE_REMOVE_EXPIRED_PAGES_DELAY_SEC > current_time) {
        //                         ^^^ to be sure page not being used by anyone
        last_data_idx = idx;
      }

      // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
      //                                                                              ^
      if (index_record.expire_at == 0) {
        break;
      }
    }

    // mark as unlinked, release this pages locally and unlink
    // [expired][expired][data][expired][data][expired][expired][expired][expired][zero][unused][unused]...[unused]
    //                   last_data_idx ---^     ^        ^        ^        ^        ^--- idx
    if (idx > 0 && last_data_idx < --idx) {
      for (; last_data_idx < idx; idx--) {
        TablePageIndexElement& index_record = table_index->shared_elements[idx];
        const auto* page = getPageByName(index_record.page_name);

        if (page == nullptr) {
          std::cerr << "ERROR Table::release_expired_memory_pages cannot acquire page:"
                    << index_record.page_name << std::endl;
          continue;
        }

        page->shared_pageinfo->unlinked = true;
        SharedMemoryPage<ELEMENT_T>::unlink(page->page_name);

        clear_index_record(index_record);
        // clear only certain portion per time;
        if (MEMPAGE_REMOVE_EXPIRED_PAGES_AT_ONCE <= ++cleared_counter) {
          break;
        }
      }
    }
  }

  lock->exit();

  if (cleared_counter > 0) {
    statsd::metric.count("dealsrv.page_use", -cleared_counter, {{"page", "new"}});
  }

  // clear opened_pages_list from unlinked items
  // all processes must do that
  std::vector<SharedMemoryPage<ELEMENT_T>*> new_pages_list;
  for (const auto page : opened_pages_list) {
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
  if (!page_name.length()) {
    std::cout << "ERROR SharedMemoryPage::SharedMemoryPage page_name empty" << std::endl;
    return;
  }

  // two processes could try to open the same page in a same time
  // first will create page, but not truncate yet, second will open it
  // compare size and remove page because it has wrong(zero) size
  locks::CriticalSection lock(page_name);
  lock.enter();
  //   ^^^^^ will auto exited on class destruction, if exited before lock.exit()

  bool new_memory_allocated = false;
  page_memory_size = sizeof(Page_information) + sizeof(ELEMENT_T) * elements;
  uint32_t aligned_pages = page_memory_size / sysconf(_SC_PAGE_SIZE);
  page_memory_size = (aligned_pages + 1) * sysconf(_SC_PAGE_SIZE);

  // try to create page
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
    if (!checkSharedMemAvailability()) {
      std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage LOW SHARED MEMORY" << std::endl;
      // dont remove file!!! shm_unlink(page_name.c_str());
      close(fd);
      return;
    }
    // if new setup page size
    int res = ftruncate(fd, page_memory_size);
    if (res == -1) {
      std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage cant truncate:" << errno << " "
                << page_name << " REMOVING..." << std::endl;
      shm_unlink(page_name.c_str());
      close(fd);
      return;
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
    return;
  }

  // page well allocated, let other processes work with this page.
  lock.exit();

  // map page to process local memory
  void* map = mmap(nullptr, page_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  // After a call to mmap(2) the file descriptor may be closed without affecting
  // the memory mapping. http://linux.die.net/man/3/shm_open
  close(fd);

  if (map == MAP_FAILED) {
    std::cerr << "ERROR SharedMemoryPage::SharedMemoryPage MAP_FAILED:" << errno << " page_name("
              << page_name << ") size:" << page_memory_size << std::endl;
    // no shm_unlink(page_name.c_str());
    return;
  }

  shared_memory = map;
  shared_pageinfo = (Page_information*)shared_memory;
  shared_elements = (ELEMENT_T*)((uint8_t*)shared_memory + sizeof(Page_information));

  if (new_memory_allocated) {
    // cleanup info structure & first element
    memset(shared_memory, 0, page_memory_size);
    shared_pageinfo->unlinked = false;
    shared_pageinfo->expiration_check = 0;
  }
  // std::cout << "MAKE PAGE: " << page_name <<  "(" << page_memory_size << ") " << std::endl;
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
// isAllocated
//------------------------------------------------------------
template <typename ELEMENT_T>
bool SharedMemoryPage<ELEMENT_T>::isAllocated() {
  return shared_memory != nullptr;
}

//------------------------------------------------------------
// operator ELEMENT_T*()
//------------------------------------------------------------
template <typename ELEMENT_T>
ElementPointer<ELEMENT_T>::operator ELEMENT_T*() {
  return get_data();
}

/*-----------------------------------------------------------------
* ElementPointer get_data
*-----------------------------------------------------------------*/
template <typename ELEMENT_T>
ELEMENT_T* ElementPointer<ELEMENT_T>::get_data() {
  if (error != ErrorCode::NO_ERROR) {
    return nullptr;
  }

  auto* page = table.getPageByName(page_name);
  if (page == nullptr) {
    std::cerr << "ERROR ElementPointer::get_data" << std::endl;
    return nullptr;
  }

  return page->getElements() + index;
}
}  // namespace shared_mem
