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

class Lock {
 protected:
  Lock(std::string name);
  ~Lock();

  void semaphore_accuire();
  void semaphore_release();
  void semaphore_reset();

 private:
  sem_t *lock;
  std::string name;
  bool initialized;
};

class CriticalSection : public Lock {
 public:
  CriticalSection(std::string name) : Lock(name), unlock_needed(false){};
  ~CriticalSection();

  void enter();
  void exit();

 private:
  bool unlock_needed;
};

int unit_test();
}

#endif