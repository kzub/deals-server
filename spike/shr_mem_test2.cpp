#include <stdio.h>

#include <errno.h>
#include <fcntl.h> /* For O_* constants */
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <iostream>

#define MEMNAME "mytest5"

long getms() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  long time_in_mill = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
  return time_in_mill;
}

void test_read_write(bool write) {
  int MEMSIZE = 10000;

  int fd;
  if (write) {
    fd = shm_open(MEMNAME, O_RDWR | O_CREAT | O_EXCL, (mode_t)0666);
  } else {
    fd = shm_open(MEMNAME, O_RDWR | O_CREAT | O_EXCL, (mode_t)0666);
  }

  if (fd == -1) {
    perror("shm_open1");
    return;
  }

  // if (write) {
  int res = ftruncate(fd, MEMSIZE);
  if (res == -1) {
    perror("TRUNC");
    return;
  }
  printf("NEW: mem truncated:%u bytes\n", MEMSIZE);
  // }

  int* map = (int*)mmap(NULL, MEMSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (write) {
    map[0] = 22;
    map[1] = 33;
    map[2] = 44;
  }

  std::cout << map[0] << map[1] << map[2] << std::endl;

  munmap(map, MEMSIZE);
  shm_unlink(MEMNAME);
}

int main() {
  shm_unlink(MEMNAME);
  test_read_write(true);
  test_read_write(false);
  return 0;

  uint32_t MEMSIZE = 2 * 1000 * 1000 * 1000 & ~(sysconf(_SC_PAGE_SIZE) - 1);
  /* offset for mmap() must be page aligned */

  printf("open mem:%s %d bytes\n", MEMNAME, MEMSIZE);
  int fd = shm_open(MEMNAME, O_RDWR | O_CREAT | O_EXCL, (mode_t)0666);
  if (fd == -1) {
    if (errno == EEXIST) {
      fd = shm_open(MEMNAME, O_RDWR | O_CREAT, (mode_t)0666);
    }
    if (fd == -1) {
      perror("vot tak");
      return -1;
    }
    printf("EXIST:\n");
  } else {
    int res = ftruncate(fd, MEMSIZE);
    if (res == -1) {
      perror("TRUNC");
      return -1;
    }
    printf("NEW: mem truncated:%u bytes\n", MEMSIZE);
  }

  // fchmod(fd, 777);
  int* map = (int*)mmap(NULL, MEMSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  printf("shared mem mapped fd:%d\n", fd);

  if (map == MAP_FAILED) {
    perror("MAP_FAILED");
    return -1;
  }

  long t0 = getms();
  printf("begin\n");

  uint32_t i;
  int c = 0;
  int r = 0;
  int v;
  int step = 27;
  for (r = 2; r < 10; r++) {
    for (i = 0; i < MEMSIZE; i += step) {
      v = *(map + i);
      if (v == 10) {
        c++;
      }
      if (v > 150) {
        c++;
      }
      if (v < 150) {
        c--;
      }
    }
  }

  ((int*)map)[3] = 3;
  long t1 = getms();
  printf("done in %ld ms\n", t1 - t0);

  munmap(map, MEMSIZE);
  shm_unlink(MEMNAME);

  return 0;
}