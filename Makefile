#
# TODO: Move `libmongoclient.a` to /usr/local/lib so this can work on production servers
#
 
#CC := g++ # This is the main compiler
# CC := clang --analyze # and comment out the linker last line for sanity
CC := clang++
SRCDIR := src
BUILDDIR := build
TARGET := bin/deals-server

SRCEXT := cpp
HEXT := hpp
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))
CFLAGS := -g -O3 -Wall -std=c++0x
LIB :=  -L lib #-lrt

INC := -I include

$(TARGET): $(OBJECTS)
	@echo " Linking..."
	@echo " $(CC) $^ -o $(TARGET) -arch x86_64 $(LIB)"; $(CC) $^ -o $(TARGET) -arch x86_64 $(LIB)

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	@echo " $(CC) $(CFLAGS) $(INC) -arch x86_64 -c -o $@ $<"; $(CC) $(CFLAGS) $(INC) -arch x86_64 -c -o $@ $<
run:
	$(TARGET)

clean:
	@echo " Cleaning..."; 
	@echo " $(RM) -r $(BUILDDIR) $(TARGET)"; $(RM) -r $(BUILDDIR) $(TARGET)

# Tests
tester:
	$(CC) $(CFLAGS) test/tester.cpp $(INC) $(LIB) -o bin/tester

# Spikes
ticket:
	$(CC) $(CFLAGS) spikes/ticket.cpp $(INC) $(LIB) -o bin/ticket

.PHONY: clean
# DO NOT DELETE

src/deals.o: /usr/include/sys/mman.h /usr/include/sys/appleapiopts.h
src/deals.o: /usr/include/sys/cdefs.h /usr/include/sys/_symbol_aliasing.h
src/deals.o: /usr/include/sys/_posix_availability.h /usr/include/sys/_types.h
src/deals.o: /usr/include/machine/_types.h /usr/include/i386/_types.h
src/deals.o: /usr/include/sys/_pthread/_pthread_types.h
src/deals.o: /usr/include/sys/_types/_mode_t.h
src/deals.o: /usr/include/sys/_types/_off_t.h
src/deals.o: /usr/include/sys/_types/_size_t.h src/deals.hpp
src/deals.o: src/shared_memory.hpp src/locks.hpp /usr/include/semaphore.h
src/deals.o: /usr/include/sys/types.h /usr/include/machine/types.h
src/deals.o: /usr/include/i386/types.h /usr/include/sys/_types/_int8_t.h
src/deals.o: /usr/include/sys/_types/_int16_t.h
src/deals.o: /usr/include/sys/_types/_int32_t.h
src/deals.o: /usr/include/sys/_types/_int64_t.h
src/deals.o: /usr/include/sys/_types/_intptr_t.h
src/deals.o: /usr/include/sys/_types/_uintptr_t.h
src/deals.o: /usr/include/machine/endian.h /usr/include/i386/endian.h
src/deals.o: /usr/include/sys/_endian.h /usr/include/libkern/_OSByteOrder.h
src/deals.o: /usr/include/libkern/i386/_OSByteOrder.h
src/deals.o: /usr/include/sys/_types/_dev_t.h
src/deals.o: /usr/include/sys/_types/_blkcnt_t.h
src/deals.o: /usr/include/sys/_types/_blksize_t.h
src/deals.o: /usr/include/sys/_types/_gid_t.h
src/deals.o: /usr/include/sys/_types/_in_addr_t.h
src/deals.o: /usr/include/sys/_types/_in_port_t.h
src/deals.o: /usr/include/sys/_types/_ino_t.h
src/deals.o: /usr/include/sys/_types/_ino64_t.h
src/deals.o: /usr/include/sys/_types/_key_t.h
src/deals.o: /usr/include/sys/_types/_nlink_t.h
src/deals.o: /usr/include/sys/_types/_id_t.h /usr/include/sys/_types/_pid_t.h
src/deals.o: /usr/include/sys/_types/_uid_t.h
src/deals.o: /usr/include/sys/_types/_clock_t.h
src/deals.o: /usr/include/sys/_types/_ssize_t.h
src/deals.o: /usr/include/sys/_types/_time_t.h
src/deals.o: /usr/include/sys/_types/_useconds_t.h
src/deals.o: /usr/include/sys/_types/_suseconds_t.h
src/deals.o: /usr/include/sys/_types/_rsize_t.h
src/deals.o: /usr/include/sys/_types/_errno_t.h
src/deals.o: /usr/include/sys/_types/_fd_def.h
src/deals.o: /usr/include/sys/_types/_fd_setsize.h
src/deals.o: /usr/include/sys/_types/_fd_set.h
src/deals.o: /usr/include/sys/_types/_fd_clr.h
src/deals.o: /usr/include/sys/_types/_fd_zero.h
src/deals.o: /usr/include/sys/_types/_fd_isset.h
src/deals.o: /usr/include/sys/_types/_fd_copy.h
src/deals.o: /usr/include/sys/_pthread/_pthread_attr_t.h
src/deals.o: /usr/include/sys/_pthread/_pthread_cond_t.h
src/deals.o: /usr/include/sys/_pthread/_pthread_condattr_t.h
src/deals.o: /usr/include/sys/_pthread/_pthread_mutex_t.h
src/deals.o: /usr/include/sys/_pthread/_pthread_mutexattr_t.h
src/deals.o: /usr/include/sys/_pthread/_pthread_once_t.h
src/deals.o: /usr/include/sys/_pthread/_pthread_rwlock_t.h
src/deals.o: /usr/include/sys/_pthread/_pthread_rwlockattr_t.h
src/deals.o: /usr/include/sys/_pthread/_pthread_t.h
src/deals.o: /usr/include/sys/_pthread/_pthread_key_t.h
src/deals.o: /usr/include/sys/_types/_fsblkcnt_t.h
src/deals.o: /usr/include/sys/_types/_fsfilcnt_t.h /usr/include/sys/fcntl.h
src/deals.o: /usr/include/Availability.h /usr/include/AvailabilityInternal.h
src/deals.o: /usr/include/sys/_types/_o_sync.h
src/deals.o: /usr/include/sys/_types/_o_dsync.h
src/deals.o: /usr/include/sys/_types/_seek_set.h
src/deals.o: /usr/include/sys/_types/_s_ifmt.h
src/deals.o: /usr/include/sys/_types/_timespec.h
src/deals.o: /usr/include/sys/_types/_filesec_t.h
src/deals.o: /usr/include/sys/semaphore.h /usr/include/fcntl.h
src/deals.o: src/shared_memory.tpp /usr/include/string.h
src/deals.o: /usr/include/_types.h /usr/include/sys/_types/_null.h
src/deals.o: /usr/include/strings.h /usr/include/secure/_string.h
src/deals.o: /usr/include/secure/_common.h /usr/include/errno.h
src/deals.o: /usr/include/sys/errno.h src/timing.hpp /usr/include/sys/time.h
src/deals.o: /usr/include/sys/_types/_timeval.h
src/deals.o: /usr/include/sys/_types/_timeval64.h /usr/include/time.h
src/deals.o: /usr/include/sys/_select.h
