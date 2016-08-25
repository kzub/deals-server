#
 
#CC := g++ # This is the main compiler
# CC := clang --analyze # and comment out the linker last line for sanity
CC := clang++
SRCDIR := src
BUILDDIR := build
TARGET_DIR := bin
TARGET_FILE := deals-server

UNAME := $(shell uname)


SRCEXT := cpp
HEXT := hpp
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))
CFLAGS := -g -O3 -Wall -Werror -std=c++14

ifeq ($(UNAME), Linux)
LIB :=  -L lib -lrt -pthread
endif
ifeq ($(UNAME), Darwin)
LIB :=  -L lib
endif

INC := -I include

$(TARGET_DIR)/$(TARGET_FILE): $(OBJECTS)
	@mkdir -p $(TARGET_DIR)
	@echo " Linking..."
	@echo " $(CC) $^ -o $(TARGET_DIR)/$(TARGET_FILE)  $(LIB)"; $(CC) $^ -o $(TARGET_DIR)/$(TARGET_FILE)  $(LIB)

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	@echo " $(CC) $(CFLAGS) $(INC)  -c -o $@ $<"; $(CC) $(CFLAGS) $(INC)  -c -o $@ $<
run:
	$(TARGET_DIR)/$(TARGET_FILE)

clean:
	@echo " Cleaning..."; 
	@echo " $(RM) -r $(BUILDDIR) $(TARGET_DIR)/$(TARGET_FILE)"; $(RM) -r $(BUILDDIR) $(TARGET_DIR)/$(TARGET_FILE)

# Tests
tester:
	$(CC) $(CFLAGS) test/tester.cpp $(INC) $(LIB) -o bin/tester


# Spikes
ticket:
	

