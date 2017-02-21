#ifndef LOCKS_HPP
#define LOCKS_HPP

#include <iostream>

#include <semaphore.h>

namespace locks {

#define WAIT_INFINITY_TIME TRUE
#define SLEEP_BETWEEN_TRIES_USEC 10
#define WAIT_FOR_LOCK_MSEC 5000

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
  void semaphore_release(bool nothrow = false);
  void check();

  std::string name;
  bool initialized;
  bool unlock_needed;
  sem_t* lock;
};

int unit_test();
}  // namespace locks

#endif