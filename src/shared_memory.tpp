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
    std::cout << "TABLE_NAME_TOO_LONG" << table_name << "(max:" << MEMPAGE_NAME_MAX_LEN - 6 << ")"
              << std::endl;
    throw "TABLE_NAME_TOO_LONG";
  }

  // open existed index or make new one
  table_index = new SharedMemoryPage<TablePageIndexElement>(table_name, table_max_pages);
  if (!table_index->isAllocated()) {
    std::cout << "CANNOT_ALLOCATE_TABLE_INDEX for: " << table_name << " pages:" << table_max_pages
              << std::endl;
    throw "CANNOT_ALLOCATE_TABLE_INDEX";
  }

  lock = new locks::CriticalSection(table_name);
}

// Table Destructor ----------------------------------------------
template <typename ELEMENT_T>
Table<ELEMENT_T>::~Table() {
  std::cout << "TABLE destructor called" << std::endl;
  // cleanup all shared memory mappings on exit
  release_open_pages();

  // delete index
  delete table_index;
  delete lock;
}

// processTablePages ----------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::process(TableProcessor<ELEMENT_T>* processor) {
  // *** Make a copy to local heap ***
  lock->enter();

  std::vector<TablePageIndexElement> records_to_scan;

  uint16_t idx = 0;
  uint16_t last_not_expired_idx = 0;
  uint32_t timestamp_now = timing::getTimestampSec();

  TablePageIndexElement* index_first = table_index->getElements();
  TablePageIndexElement* index_current;

  // search for pages to scan (not expired)
  for (idx = 0; idx < table_max_pages; idx++) {
    // current page row (pointer to shared memory)
    index_current = index_first + idx;

    // if page not empty and not expired
    if (index_current->expire_at >= timestamp_now) {
      records_to_scan.push_back(*index_current);
      last_not_expired_idx = idx;
    }
    // or not used yet
    else if (index_current->expire_at == 0) {
      // stop here. next pages are unused
      break;
    } else {
      // or if page expired add it to list
      // unlinked_records.push_back(*index_current);
    }
  }

  uint16_t cleaning_idx = last_not_expired_idx + 1;
  // std::cout << "(PROCESS) cleaning_idx:" << cleaning_idx << " idx:" << idx
  //          << std::endl;

  // remove expired pages at the table end
  // first page will never released
  // let's keep it allocated
  if (cleaning_idx < idx) {
    for (; cleaning_idx < idx; cleaning_idx++) {
      // current page row (pointer to shared memory)
      index_current = index_first + cleaning_idx;
      // mark as deleted in shared mem
      index_current->expire_at = 0;

      SharedMemoryPage<ELEMENT_T>* page = getPageByName(index_current->page_name);
      page->shared_pageinfo->unlinked = true;
      // The operation of shm_unlink() removes a shared memory object name, and,
      // once all processes have unmapped the object, de-allocates and destroys
      // the contents of the associated memory region.
      // http://linux.die.net/man/3/shm_open
      SharedMemoryPage<ELEMENT_T>::unlink(index_current->page_name);
    }
    // force running local page clearing...
    idx = cleaning_idx;
  }

  lock->exit();

  // check pages amount
  if (last_known_index_length > idx) {
    index_current = index_first + last_not_expired_idx;
    SharedMemoryPage<ELEMENT_T>* good_page = localGetPageByName(index_current->page_name);

    // if page exists localy => it was allocated and we could pop() open_pages
    // till we meet it.
    if (good_page != nullptr) {
      for (;;) {
        // get last element
        SharedMemoryPage<ELEMENT_T>* page_to_unmap = opened_pages_list.back();
        if (page_to_unmap->page_name == good_page->page_name) {
          break;
        }
        // not target page => unmap it;
        delete page_to_unmap;
        opened_pages_list.pop_back();
      }
    }
  }

  // update lastknow index
  last_known_index_length = idx;

  // and go to process pages:
  SharedMemoryPage<ELEMENT_T>* page_to_process;
  bool continue_iteration;

  // std::vector<TablePageIndexElement>::iterator it;
  // for (it = records_to_scan.begin(); it != records_to_scan.end(); ++it) {
  //   page_to_process = getPageByName((*it).page_name);
  //   // std::cout << "PAGE:" << it - records_to_scan.begin() << std::endl;
  //   continue_iteration = processor->process_function(
  //       page_to_process->getElements(), max_elements_in_page - (*it).page_elements_available);
  //   if (continue_iteration == false) {
  //     break;
  //   }
  // }
  for (auto it : records_to_scan) {
    page_to_process = getPageByName(it.page_name);
    // std::cout << "PAGE:" << it - records_to_scan.begin() << std::endl;
    continue_iteration = processor->process_function(
        page_to_process->getElements(), max_elements_in_page - it.page_elements_available);
    if (continue_iteration == false) {
      break;
    }
  }
}

// cleanup ----------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::cleanup() {
  release_open_pages();

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
      index_current->expire_at = 0;
      index_current->page_elements_available = max_elements_in_page;
      // SharedMemoryPage<ELEMENT_T>* page =
      // getPageByName(index_current->page_name);
      SharedMemoryPage<ELEMENT_T>::unlink(index_current->page_name);
    } else {
      // stop here. next pages are unused
      break;
    }
  }

  lock->exit();

  SharedMemoryPage<ELEMENT_T>::unlink(table_index->page_name);
}

// release_open_pages ----------------------------------------------
template <typename ELEMENT_T>
void Table<ELEMENT_T>::release_open_pages() {
  // typename std::vector<SharedMemoryPage<ELEMENT_T>*>::iterator page;

  // for (page = opened_pages_list.begin(); page != opened_pages_list.end(); ++page) {
  //   delete (*page);
  // }

  for (auto page : opened_pages_list) {
    delete page;
  }
}

// Table addRecord ----------------------------------------------
template <typename ELEMENT_T>
ElementPointer<ELEMENT_T> Table<ELEMENT_T>::addRecord(ELEMENT_T* records_pointer,
                                                      uint32_t records_cout,
                                                      uint32_t lifetime_seconds) {
  if (records_cout > max_elements_in_page) {
    return ElementPointer<ELEMENT_T>(*this, ErrorCode::RECORD_SIZE_TO_BIG);
  }

  std::string insert_page_name;
  uint32_t insert_element_idx;
  uint32_t current_time = timing::getTimestampSec();
  uint16_t idx = 0;
  TablePageIndexElement* index_record;
  // std::cout << "CURRENT_TIME: " << current_time << std::endl;
  lock->enter();

  // search for free space in pages
  for (idx = 0; idx < table_max_pages; idx++) {
    // current page row (pointer to shared memory)
    index_record = &table_index->shared_elements[idx];
    // std::cout << "===> " << table_index->page_name + ":" +
    // std::to_string(idx) << " AVAIL:" << index_record->page_elements_available
    // << " expire_at:" << std::to_string(index_record->expire_at) << std::endl;

    bool current_record_was_cleared = false;
    // page expired -> make it empty and use it to save records
    if (index_record->expire_at > 0 && index_record->expire_at < current_time) {
      index_record->expire_at = 0;
      current_record_was_cleared = true;
      // std::cout << "(ADD) EXPIRED PAGES:" << index_record->page_name  <<
      // std::endl;
    }

    // page exist and not fit (go next)
    if (index_record->expire_at > 0 && index_record->page_elements_available < records_cout) {
      continue;
    }

    insert_page_name = table_index->page_name + ":" + std::to_string(idx);

    // page is empty -> use it
    if (index_record->expire_at == 0) {
      // page still has full capacity, lets insert at the begining
      insert_element_idx = 0;
      std::cout << "USE EMPTY page:" << insert_page_name << std::endl;

      // calculate capacity after we will put records
      index_record->page_elements_available = max_elements_in_page - records_cout;
      // copy page_name to shared meme
      std::memcpy(index_record->page_name, insert_page_name.c_str(), insert_page_name.length());

      // fill with zero next index row in case last row  has no space
      if (!current_record_was_cleared && idx < table_max_pages - 1) {
        table_index->shared_elements[idx + 1].expire_at = 0;
      }
    }
    // or use current page
    else {
      // std::cout << "use AVAIL page:" << std::endl;
      // max_elements_in_page stored in table
      // must be the same on all program instances
      insert_element_idx = max_elements_in_page - index_record->page_elements_available;
      // decrease available elements
      index_record->page_elements_available -= records_cout;
    }

    uint32_t expire_time;
    if (lifetime_seconds != 0) {
      expire_time = current_time + lifetime_seconds;
    } else {
      // page will expire after N seconds
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
    return ElementPointer<ELEMENT_T>(*this, ErrorCode::NO_SPACE_TO_INSERT);
  }

  // now we have page to insert
  // and position to insert
  // let's look for page now in local heap or allocate it
  SharedMemoryPage<ELEMENT_T>* page = getPageByName(insert_page_name);

  if (!page->isAllocated()) {
    return ElementPointer<ELEMENT_T>(*this, ErrorCode::CANT_FIND_PAGE);
  }

  // copy array of (records_cout) elements to shared memeory
  std::memcpy(&page->shared_elements[insert_element_idx], records_pointer,
              sizeof(ELEMENT_T) * records_cout);

  // std::cout << "COPY :" << insert_page_name << " idx:" << insert_element_idx
  // << " cout:" << records_cout <<	" size:" <<
  // sizeof(ELEMENT_T)*records_cout
  // << std::endl;
  return ElementPointer<ELEMENT_T>(*this, insert_page_name, insert_element_idx, records_cout);
}

// Table localGetPageByName -------------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>* Table<ELEMENT_T>::localGetPageByName(std::string page_name_to_look) {
  // find page in open pages list
  // typename std::vector<SharedMemoryPage<ELEMENT_T>*>::iterator page;
  // for (page = opened_pages_list.begin(); page != opened_pages_list.end(); ++page) {
  //   if ((*page)->page_name == page_name_to_look) {
  //     return *page;
  //   }
  // }

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
  if (page == nullptr) {
    page = new SharedMemoryPage<ELEMENT_T>(page_name_to_look, max_elements_in_page);

    if (!page->isAllocated()) {
      delete page;
      return nullptr;
    }

    // close handlers when destroyed
    opened_pages_list.push_back(page);
  }

  return page;
}

/*-----------------------------------------------------------------
* SHARED MEMORY
*-----------------------------------------------------------------*/

// SharedMemoryPage Constructor ----------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>::SharedMemoryPage(std::string page_name, uint32_t elements)
    : page_name(page_name), shared_memory(nullptr) {
  if (!page_name.length()) {
    std::cout << "page_name error" << std::endl;
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
      std::cout << "Cannot create or open memory page:" << errno << " " << page_name << std::endl;
      return;
    }

    std::cout << "OPENED:" << page_name << std::endl;
  } else {
    // if new setup page size
    int res = ftruncate(fd, page_memory_size);
    if (res == -1) {
      std::cout << "ERROR: cant truncate:" << errno << " " << page_name << std::endl;
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
    std::cerr << "MAP_FAILED:" << errno << page_name << " size:" << page_memory_size << std::endl;
    return;
  }

  shared_memory = map;
  shared_pageinfo = (page_information*)shared_memory;
  shared_elements = (ELEMENT_T*)((uint8_t*)shared_memory + sizeof(page_information));

  if (new_memory_allocated) {
    // cleanup info structure & first element
    memset(shared_memory, 0, sizeof(page_information) + sizeof(ELEMENT_T));
    shared_pageinfo->unlinked = false;
    // std::cout << "MEMSET:" << page_name << " bytes:" <<
    // sizeof(page_information) + sizeof(ELEMENT_T) << std::endl;
  }
  // // std::cout << "MAKE PAGE: " << page_name <<  "(" << page_memory_size <<
  // ") " << std::endl;
};

// SharedMemoryPage Destructor -----------------------------------
template <typename ELEMENT_T>
SharedMemoryPage<ELEMENT_T>::~SharedMemoryPage() {
  // The munmap() system call deletes the mappings for the specified address
  // range, and causes further references to addresses within the range to
  // generate invalid memory references. The region is also automatically
  // unmapped when the process is terminated. On the other hand, closing the
  // file descriptor does not unmap the region.
  int res_unmap = munmap(shared_memory, page_memory_size);
  std::cout << "FREE " << page_name << "(" << page_memory_size << ") "
            << (res_unmap == 0 ? "OK " : "FAIL ") << std::endl;
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

  return table.getPageByName(page_name)->getElements() + index;
}
}
