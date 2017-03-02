#ifndef LOCKS_HPP
#define LOCKS_HPP

#include <iostream>

#include <semaphore.h>
#include "types.hpp"

namespace locks {
#define WAIT_FOR_LOCK_SEC 30
#ifdef __APPLE__
int sem_timedwait(sem_t* lock, const struct timespec* abs_timeout);
#endif

class CriticalSection {
 public:
  CriticalSection(std::string name);
  ~CriticalSection();

  void enter();
  void exit();
  bool is_locked();
  void reset_not_for_production();

 private:
  void semaphore_accuire();
  void semaphore_release();

  std::string name;
  bool unlock_needed = false;
  sem_t* lock = nullptr;
};

class AutoCloser {
 public:
  AutoCloser(CriticalSection& cs) : cs(cs) {
  }
  ~AutoCloser();
  CriticalSection& cs;
};

const struct timespec operation_timeout { WAIT_FOR_LOCK_SEC, 0 };

int unit_test();
}  // namespace locks

#endif