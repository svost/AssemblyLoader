SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .o .cxx .hxx .h

# Default: g++ , as option USE_CLANG=1 or USE_MINGW=1 or USE_ICPC=1
CXX=g++
CLSPECIFIC=-Og -Winline

EXEC=picovm

ifeq (${USE_CLANG}, 1)
    CXX=clang++
    CLSPECIFIC=
endif
ifeq (${USE_WCLANG}, 1)
    CXX=clang++
    CLSPECIFIC=-O2 --target=x86_64-w64-mingw32 -static -DWIN32=1
    EXEC:=$(EXEC).exe
endif
ifeq (${USE_MINGW}, 1)
    CXX=x86_64-w64-mingw32-g++
    CLSPECIFIC=-Og -static -DWIN32=1
    EXEC:=$(EXEC).exe
endif
ifeq (${USE_ICPC}, 1)
    CXX=icpc
    CLSPECIFIC=
endif

CXXFLAGS=-g -std=c++11 -Wall -Wextra -Wshadow $(CLSPECIFIC)

SOURCES=\
    PicoVM/main.cxx \
    $(wildcard PicoVM/CLR/*.cxx) \
    PicoVM/CLR/crossguid/guid.cxx
INCDIRS=PicoVM/CLR/
OBJECTS=$(SOURCES:.cxx=.o)
DEPS=$(OBJECTS:.o=.d)

.PHONY: clean

all: $(EXEC)

$(EXEC): $(OBJECTS)
	@ echo "LD  " $(notdir $@)
	@ $(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cxx
	@ echo "CXX " $(notdir $<)
	@ $(CXX) $(CXXFLAGS) -I $(INCDIRS) -c -MMD -MP -o $@ $<

clean:
	@ -rm -rf $(EXEC) $(OBJECTS) $(DEPS) $(EXEC).exe

-include $(DEPS)