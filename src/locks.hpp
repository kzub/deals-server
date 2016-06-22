#ifndef LOCKS_HPP
#define LOCKS_HPP

#include <semaphore.h>
#include <iostream>

#include <cinttypes>

#include <fcntl.h> /* For O_* constants */
#include <semaphore.h>

namespace locks {

#define SLEEP_BETWEEN_TRIES_USEC 1000
#define WAIT_FOR_LOCK_MSEC 5000

class CriticalSection {
 public:
  CriticalSection(std::string name);
  ~CriticalSection();

  void enter();
  void exit();

 private:
  void semaphore_accuire();
  void semaphore_release(bool nothrow = false);
  void check();

  std::string name;
  bool initialized;
  bool unlock_needed;
  sem_t *lock;
};

int unit_test();
}

#endif