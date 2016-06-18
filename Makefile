#
# TODO: Move `libmongoclient.a` to /usr/local/lib so this can work on production servers
#
 
#CC := g++ # This is the main compiler
# CC := clang --analyze # and comment out the linker last line for sanity
CC := clang++
SRCDIR := src
BUILDDIR := build
TARGET := bin/deals-server

UNAME := $(shell uname)


SRCEXT := cpp
HEXT := hpp
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))
CFLAGS := -g -O3 -Wall -std=c++0x

ifeq ($(UNAME), Linux)
LIB :=  -L lib -lrt
endif
ifeq ($(UNAME), Darwin)
LIB :=  -L lib #-lrt
endif

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
	

