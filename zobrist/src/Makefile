#debug = yes

TARGET = zobrist

srcdir ?= .
debugdir = ../debug
releasedir = ../release

ifeq ($(debug),yes)
	builddir ?= $(debugdir)
	CXXFLAGS += -DDEBUG -g
else
	builddir ?= $(releasedir)
	CXXFLAGS += -DNDEBUG
endif

LIBS = -pthread
OPTIONS  = -std=c++0x -mssse3 -march=native -mtune=native
OPTIONS += -finline-functions -funroll-all-loops

OPTIMIZATIONS = -Ofast -flto -fno-rtti -fno-common
WARNINGS += -pedantic -Wall -Wextra -Wuninitialized -Wpointer-arith -Wcast-qual -Wcast-align
//WARNINGS += -Wsign-conversion
//WARNINGS += -Wconversion -Wshadow
WARNINGS += -Wno-sign-compare

CXXFLAGS += $(OPTIONS)
CXXFLAGS += $(OPTIMIZATIONS)
CXXFLAGS += $(WARNINGS)
LDFLAGS += $(LIBS)
LDFLAGS += $(OPTIMIZATIONS)

SOURCES := $(wildcard $(srcdir)/*.cpp)
OBJECTS := $(patsubst $(srcdir)/%.cpp, $(builddir)/%.o, $(SOURCES))
DEPS := $(patsubst %.o, %.d, $(OBJECTS))

-include $(DEPS)

$(builddir)/$(TARGET): $(PRECOMP) $(OBJECTS)
	$(CXX) -o $@ $(LDFLAGS) $(OBJECTS)

$(builddir)/%.o: $(srcdir)/%.cpp | $(builddir)
	$(CXX) $< -c -o $@ -MD $(CXXFLAGS)

$(builddir):
	mkdir -p $@

.PHONY: all clean

all: $(builddir)/$(TARGET)

clean:
	$(RM) -r *.o *.gch *.d $(debugdir) $(releasedir)
