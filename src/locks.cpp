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
CriticalSection::CriticalSection(std::string name) : name(name) {
  lock = sem_open(name.c_str(), O_RDWR | O_CREAT, (mode_t)0666, 1);

  if (lock == SEM_FAILED) {
    std::cerr << "ERROR CriticalSection::CriticalSection sem_open() errno:" << errno << std::endl;
    throw types::Error("LOCK_SEM_FAILED\n", types::ErrorCode::InternalError);
  }
}

//-----------------------------------------------
// CriticalSection Destructor
//-----------------------------------------------
CriticalSection::~CriticalSection() {
  if (unlock_needed) {
    semaphore_release();
  }
  sem_close(lock);
};

//-----------------------------------------------
// sem_timedwait definiton for osx
//-----------------------------------------------
#ifdef __APPLE__
#define SLEEP_BETWEEN_TRIES_USEC 1000
int sem_timedwait(sem_t* alock, const struct timespec* abs_timeout) {
  uint32_t wait_retries = 0;
  while (1) {
    int res = sem_trywait(alock);
    if (res == -1) {
      wait_retries++;
      if (wait_retries * SLEEP_BETWEEN_TRIES_USEC / 1000000 > WAIT_FOR_LOCK_SEC) {
        return -1;
      }
      usleep(SLEEP_BETWEEN_TRIES_USEC);
      continue;
    }
    return 0;
  }
  return 0;
}
#endif

//-----------------------------------------------
// CriticalSection acciure
//-----------------------------------------------
void CriticalSection::semaphore_accuire() {
  int res = sem_timedwait(lock, &operation_timeout);
  if (res == -1) {
    std::cerr << "ERROR: CriticalSection::semaphore_accuire sem_timedwait() errno:" << errno
              << std::endl;
    throw types::Error("CANT_LOCK\n", types::ErrorCode::InternalError);
  }
}

//-----------------------------------------------
// CriticalSection release
//-----------------------------------------------
void CriticalSection::semaphore_release() {
  int res = sem_post(lock);
  if (res == -1) {
    std::cout << "ERROR CriticalSection::semaphore_release sem_post() errno:" << errno << std::endl;
    throw types::Error("CANT_SEM_POST\n", types::ErrorCode::InternalError);
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
// CriticalSection enter()
//-----------------------------------------------
void CriticalSection::enter() {
  semaphore_accuire();
  unlock_needed = true;
}

//-----------------------------------------------
// CriticalSection exit()
//-----------------------------------------------
void CriticalSection::exit() {
  if (unlock_needed == true) {
    semaphore_release();
  }
  unlock_needed = false;
}

//-----------------------------------------------
// CriticalSection is_locked
//-----------------------------------------------
bool CriticalSection::is_locked() {
  return unlock_needed;
}

//-----------------------------------------------
// AutoCloser destructor
//-----------------------------------------------
AutoCloser::~AutoCloser() {
  cs.exit();
}

void testf2(CriticalSection& lock) {
  AutoCloser guard{lock};
  std::cout << "Lock: inside testf2()" << std::endl;
}

//-----------------------------------------------
// Testing...
//-----------------------------------------------
void testf(bool& second_enter) {
  // lock/unlock test
  CriticalSection lock("dbread");
  lock.reset_not_for_production();

  uint32_t start_time = timing::getTimestampSec();
  std::cout << "Lock: BEFORE 1" << std::endl;
  lock.enter();
  std::cout << "Lock: IAM IN 1" << std::endl;
  lock.exit();
  uint32_t start_time2 = timing::getTimestampSec();
  std::cout << "Lock: IAM OUT 1" << std::endl;
  assert(start_time2 - start_time < WAIT_FOR_LOCK_SEC);

  // auto unlock test
  lock.enter();
  std::cout << "Lock: IAM IN 2" << std::endl;
  testf2(lock);
  std::cout << "Lock: IAM OUT 2" << std::endl;

  // timeout test
  start_time = timing::getTimestampSec();
  lock.enter();
  std::cout << "Lock: IAM IN 3" << std::endl;

  CriticalSection lock2("dbread");
  std::cout << "Lock: TRY TO ENTER LOCKED SECTION..." << std::endl;
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
    std::cout << "Lock: Timeout exception catched" << std::endl;
    exception = true;
  }

  assert(second_enter == true);
  assert(exception == true);
  std::cout << "Locks: OK" << std::endl;
  return 0;
}
}  // namespace locks