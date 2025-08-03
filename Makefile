#debug = yes
CXX = clang++
#CXX = g++
EXE = petrel

SRCDIR ?= src
BUILD_DIR ?= build
RELEASE_DIR ?= release
TEST_DIR ?= test

ifeq ($(debug),yes)
	CXXFLAGS += -DDEBUG -ggdb
	OPTIMIZATIONS = -Og -O0
else
#	CXXFLAGS += -DNDEBUG
	CXXFLAGS += -DDEBUG -ggdb
	OPTIMIZATIONS = -Ofast -flto -finline-functions
endif

TARGET ?= $(BUILD_DIR)/$(EXE)
EXPECT ?= $(TEST_DIR)/expect.sh

CXXFLAGS += -std=c++23 -mssse3 -march=native -mtune=native
CXXFLAGS += -fno-exceptions -fno-rtti

WARNINGS += -Wall -Wpedantic -Wextra
WARNINGS += -Wno-ignored-attributes
WARNINGS += -Wuninitialized -Wcast-qual -Wconversion -Wshadow -Wmissing-declarations -Wstrict-aliasing=1 -Wstrict-overflow=5
WARNINGS += -Wpacked -Wdisabled-optimization -Wredundant-decls -Winvalid-constexpr -Wextra-semi -Wsuggest-override
#WARNINGS += -Winline -Wsign-promo

ifeq ($(CXX), g++)
	WARNINGS += -Wno-class-memaccess
	WARNINGS += -Wuseless-cast -Wcast-align=strict -Wunsafe-loop-optimizations -Wsuggest-final-types -Wsuggest-final-methods
	WARNINGS += -Wnormalized -Wvector-operation-performance
endif

ifeq ($(CXX), clang)
	CXXFLAGS += -ferror-limit=10
	WARNINGS += -Wcast-align -Wconditional-uninitialized -Wmissing-prototypes
endif

CXXFLAGS += $(OPTIMIZATIONS) $(WARNINGS)

LDLIBS += -pthread
LDFLAGS += $(LDLIBS) $(OPTIMIZATIONS) -Wl,--no-as-needed

HEADER = StdAfx.hpp
PRECOMP = $(BUILD_DIR)/$(HEADER).gch
HEADER_SRC = $(SRCDIR)/$(HEADER)

SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(patsubst $(SRCDIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))
DEPS = $(patsubst %.o, %.d, $(OBJECTS))


GIT_DATE := $(shell git log -1 --date=short --pretty=format:%cd)
ifneq ($(GIT_DATE), )
#	CXXFLAGS += -DGIT_DATE=\"$(GIT_DATE)\"
endif

GIT_SHA := $(shell git log -1 --date=short --pretty=format:%h)
ifneq ($(GIT_SHA), )
	CXXFLAGS += -DGIT_SHA=\"$(GIT_SHA)\"
endif

GIT_ORIGIN := $(shell git remote get-url origin)
ifneq ($(GIT_ORIGIN), )
	CXXFLAGS += -DGIT_ORIGIN=\"$(GIT_ORIGIN)\"
endif


.PHONY: all clean _clear_console test test-hash

all: _clear_console $(TARGET)

clean:
	$(RM) -r $(BUILD_DIR)

_clear_console:
	clear

run: all
	clear
	$(TARGET)

test: all
	$(EXPECT) $(TARGET) $(TEST_DIR)/test.rc

test-hash: all
	$(EXPECT) $(TARGET) $(TEST_DIR)/test-hash.rc

$(TARGET): $(PRECOMP) $(OBJECTS)
	$(CXX) -o $@ $(LDFLAGS) $(OBJECTS)

$(BUILD_DIR)/%.o: $(SRCDIR)/%.cpp $(PRECOMP)
	$(CXX) -c -o $@ $< -MMD $(CXXFLAGS) -Winvalid-pch -include $(HEADER_SRC)

$(PRECOMP): $(HEADER_SRC) | $(BUILD_DIR)
	$(CXX) -o $@ $< -MD $(CXXFLAGS)

$(BUILD_DIR):
	mkdir -p $@

-include $(DEPS)
