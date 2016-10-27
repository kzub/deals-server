#include "locks.hpp"

#include <cassert>
#include <cinttypes>
#include <iostream>

#include <fcntl.h> /* For O_* constants */
#include <semaphore.h>
#include <unistd.h>

#include "timing.hpp"

namespace locks {

//-----------------------------------------------
// CriticalSection Constructor
//-----------------------------------------------
CriticalSection::CriticalSection(std::string name)
    : name(name), initialized(false), unlock_needed(false) {
  lock = sem_open(name.c_str(), O_RDWR | O_CREAT, (mode_t)0666, 1);

  if (lock == SEM_FAILED) {
    std::cout << "ERROR CriticalSection::CriticalSection sem_open() errno:" << errno << std::endl;
    return;
  }

  initialized = true;
}

//-----------------------------------------------
// CriticalSection Destructor
//-----------------------------------------------
CriticalSection::~CriticalSection() {
  if (unlock_needed) {
    std::cout << "ERROR: CriticalSection::~CriticalSection auto unlocking:" << name << std::endl;
    semaphore_release(true);
  }

  if (initialized) {
    sem_close(lock);
  }
};

//-----------------------------------------------
// CriticalSection acciure
//-----------------------------------------------
void CriticalSection::semaphore_accuire() {
#ifdef WAIT_INFINITY_TIME  // faster switching
  int res = sem_wait(lock);
  if (res == -1) {
    std::cerr << "ERROR: CriticalSection::semaphore_accuire sem_wait() errno:" << errno
              << std::endl;
    throw "cant unlock";
  }
  return;
#else  // ------- without WAIT_INFINITY_TIME manual implementation
  uint32_t wait_retries = 0;
  while (1) {
    int res = sem_trywait(lock);
    if (res == -1) {
      wait_retries++;

      if (wait_retries * SLEEP_BETWEEN_TRIES_USEC / 1000 > WAIT_FOR_LOCK_MSEC) {
        std::cerr << "ERROR: CriticalSection::semaphore_accuire sem_trywait() errno:" << errno
                  << std::endl;
        throw "cant unlock";
      }

      usleep(SLEEP_BETWEEN_TRIES_USEC);
      continue;
    }

    break;
  }
#endif
}

//-----------------------------------------------
// CriticalSection release
//-----------------------------------------------
void CriticalSection::semaphore_release(bool nothrow) {
  int res = sem_post(lock);
  if (res == -1) {
    std::cout << "ERROR CriticalSection::semaphore_release sem_post() errno:" << errno << std::endl;
    if (nothrow) {
      return;
    }
    throw "semaphore_release(): cannot sem_post";
  }
}

//-----------------------------------------------
// Used to clear semaphore in case you kill application, that was in locked state
//-----------------------------------------------
void CriticalSection::reset_not_for_production() {
  std::cout << "WARNING: reset_not_for_production use (" << name << ") done" << std::endl;
  while (sem_trywait(lock) == -1) {
    sem_post(lock);
  }
  sem_post(lock);
}

//-----------------------------------------------
// CriticalSection check
//-----------------------------------------------
void CriticalSection::check() {
  if (!initialized) {
    std::cerr << "ERROR CriticalSection::check not initialized" << std::endl;
    throw "check(): uninitialized";
  }
}

//-----------------------------------------------
// CriticalSection enter()
//-----------------------------------------------
void CriticalSection::enter() {
  check();
  semaphore_accuire();
  unlock_needed = true;
}

//-----------------------------------------------
// CriticalSection exit()
//-----------------------------------------------
void CriticalSection::exit() {
  check();
  semaphore_release();
  unlock_needed = false;
}

//-----------------------------------------------
// Testing...
//-----------------------------------------------
void testf(bool& second_enter) {
  //-------------------------------
  // test1
  CriticalSection lock("dbread");
  lock.reset_not_for_production();

  uint32_t start_time = timing::getTimestampSec();
  lock.enter();
  std::cout << "IAM IN 1" << std::endl;
  lock.exit();
  uint32_t start_time2 = timing::getTimestampSec();
  std::cout << "IAM OUT 1" << std::endl;
  assert(start_time2 - start_time < WAIT_FOR_LOCK_MSEC / 1000);

  //-------------------------------
  // test2
  start_time = timing::getTimestampSec();
  lock.enter();
  std::cout << "IAM IN 2" << std::endl;

  CriticalSection lock2("dbread");
  std::cout << "TRY TO ENTER LOCKED SECTION..." << std::endl;
  second_enter = true;
  lock2.enter();

  assert(true == false /*can't be here*/);
}

int unit_test() {
  bool exception = false;
  bool second_enter = false;
  try {
    testf(second_enter);
  } catch (...) {
    exception = true;
  }

  assert(second_enter == true);
  assert(exception == true);
  std::cout << "locks... OK" << std::endl;
  return 0;
}
}  // namespace locks