#include <iostream>

#include <cinttypes>

#include <fcntl.h> /* For O_* constants */
#include <semaphore.h>

#include "locks.hpp"

namespace locks {

Lock::Lock(std::string name) : name(name), initialized(false) {
  lock = sem_open(name.c_str(), O_RDWR | O_CREAT, (mode_t)0666, 1);
  if (lock == SEM_FAILED) {
    std::cout << "error opening semaphore errorcode:" << errno << std::endl;
    return;
  }

  initialized = true;

  try {
    semaphore_accuire();
  } catch (...) {
    semaphore_reset();

    // retry
    try {
      semaphore_accuire();
    } catch (...) {
      std::cout << "error accuring semaphore:" << std::endl;
      initialized = false;
      sem_close(lock);
      return;
    }
  }

  semaphore_release();
}

Lock::~Lock() {
  if (initialized) {
    sem_close(lock);
  }
}

void Lock::semaphore_reset() {
  std::cout << "FORCE to reset semaphore " << name << std::endl;

  while (sem_trywait(lock) == EAGAIN) {
    int res = sem_post(lock);
    if (res != 0) {
      std::cout << "semaphore reset errorcode:" << errno << std::endl;
      return;
    }
  }
  sem_post(lock);

  std::cout << "semaphore cleared!" << std::endl;
}

void Lock::semaphore_accuire() {
  if (!initialized) {
    std::cout << "Error: not initialized" << std::endl;
    throw "semaphore_accuire(): uninitialized";
  }

  uint32_t wait_retries = 0;
  while (1) {
    int res = sem_trywait(lock);
    if (res == -1) {
      wait_retries++;

      if (wait_retries * SLEEP_BETWEEN_TRIES_USEC / 1000 > WAIT_FOR_LOCK_MSEC) {
        std::cout << "semaphore wait timeout errorcode:" << errno << std::endl;
        throw "cant unlock";
      }

      usleep(SLEEP_BETWEEN_TRIES_USEC);
      continue;
    }

    break;
  }
}

void Lock::semaphore_release() {
  if (!initialized) {
    std::cout << "Error: not initialized" << std::endl;
    throw "semaphore_release(): uninitialized";
  }

  int res = sem_post(lock);
  if (res == -1) {
    std::cout << "error post() semaphore errorcode:" << errno << std::endl;
    throw "semaphore_release(): cannot sem_post";
  }
}

CriticalSection::~CriticalSection() {
  if (unlock_needed) {
    std::cout << "something go wrong, auto unlocking CriticalSection ..."
              << std::endl;
    semaphore_release();
  }
};

void CriticalSection::enter() {
  unlock_needed = true;
  semaphore_accuire();
}

void CriticalSection::exit() {
  semaphore_release();
  unlock_needed = false;
}

void testf() {
  CriticalSection lock("dbread");

  lock.enter();

  std::cout << "IAM IN" << std::endl;
  sleep(1);

  lock.exit();
}
int test() {
  try {
    testf();
  } catch (...) {
    std::cout << "catched" << std::endl;
  }

  return 0;
}
}