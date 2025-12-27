# === Configuration ===
EXE = petrel
BUILD_DIR ?= build
TARGET ?= $(BUILD_DIR)/$(EXE)

CXX = clang++
#CXX = g++
RM = rm -rf
MKDIR = mkdir -p
SRC_DIR ?= src
TEST_DIR ?= tests/integration
EXPECT ?= $(TEST_DIR)/expect.sh
UNIT_TEST_DIR = tests/unit

# === Default Flags (Release) ===
BUILD_FLAGS = -O3 -flto -finline-functions -DNDEBUG -static

ifeq ($(CXX), g++)
	BUILD_FLAGS += -flto=auto
endif

# === Tag Files for Build Type ===
TAG_TEST  = $(BUILD_DIR)/.test
TAG_DEBUG = $(BUILD_DIR)/.debug

# === Set Flags Based on Tags ===
ifeq ($(wildcard $(TAG_TEST)), $(TAG_TEST))
	BUILD_FLAGS = -Og -ggdb -DDEBUG
	BUILD_FLAGS = -Og -ggdb -DDEBUG -fsanitize=address,undefined
else ifeq ($(wildcard $(TAG_DEBUG)), $(TAG_DEBUG))
	BUILD_FLAGS = -O0 -ggdb -DDEBUG
	BUILD_FLAGS = -O0 -ggdb -DDEBUG -fsanitize=address,undefined
endif

CXXFLAGS = $(BUILD_FLAGS) -std=c++20 -mssse3 -march=native -mtune=native -fno-exceptions -fno-rtti

GIT_DATE := $(shell git log -1 --date=short --pretty=format:%cd 2>/dev/null || true)
ifneq ($(GIT_DATE),)
	CXXFLAGS += -DGIT_DATE=\"$(GIT_DATE)\"
endif

GIT_SHA := $(shell git log -1 --date=short --pretty=format:%h 2>/dev/null || true)
ifneq ($(GIT_SHA),)
	CXXFLAGS += -DGIT_SHA=\"$(GIT_SHA)\"
endif

GIT_ORIGIN := $(shell git remote get-url origin 2>/dev/null || true)
ifneq ($(GIT_ORIGIN),)
	CXXFLAGS += -DGIT_ORIGIN=\"$(GIT_ORIGIN)\"
endif

WARNINGS = -Wall -Wpedantic -Wextra
WARNINGS += -Wno-ignored-attributes
WARNINGS += -Wuninitialized -Wcast-qual -Wshadow -Wmissing-declarations -Wstrict-aliasing=1 -Wstrict-overflow=1 -Wsign-promo
WARNINGS += -Wpacked -Wdisabled-optimization -Wredundant-decls -Wextra-semi -Wsuggest-override
#WARNINGS += -Winline

ifeq ($(CXX), g++)
	CXXFLAGS += -flax-vector-conversions
	WARNINGS += -Wno-class-memaccess -Wno-invalid-constexpr
	WARNINGS += -Wuseless-cast -Wcast-align=strict -Wsuggest-final-types -Wsuggest-final-methods
	WARNINGS += -Wnormalized -Wunsafe-loop-optimizations -Wvector-operation-performance
else ifeq ($(CXX), clang)
	WARNINGS += -Wcast-align -Wconditional-uninitialized -Wmissing-prototypes -Wconversion -Winvalid-constexpr
endif

CXXFLAGS += $(WARNINGS)

# === Linker Flags ===
LDLIBS += -pthread
LDFLAGS += $(LDLIBS) $(BUILD_FLAGS) -Wl,--no-as-needed

# === Precompiled Header ===
HEADER = StdAfx.hpp
PRECOMP = $(BUILD_DIR)/$(HEADER).gch
HEADER_SRC = $(SRC_DIR)/$(HEADER)

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))
DEPS = $(patsubst %.o, %.d, $(OBJECTS))

MAKE_TARGET = make --jobs --warn-undefined-variables --no-print-directory $(TARGET)

.PHONY: default release test debug clean run check _clear_console

default: _clear_console $(BUILD_DIR) $(TARGET)

release: $(BUILD_DIR)
	if [ -f $(TAG_TEST) ] || [ -f $(TAG_DEBUG) ]; then $(RM) $(BUILD_DIR); fi
	@$(MKDIR) $(BUILD_DIR)
	$(MAKE_TARGET)

test: $(BUILD_DIR)
	if [ ! -f $(TAG_TEST) ]; then $(RM) $(BUILD_DIR); fi
	@$(MKDIR) $(BUILD_DIR)
	@touch $(TAG_TEST)
	$(MAKE_TARGET)

debug: $(BUILD_DIR)
	if [ ! -f $(TAG_DEBUG) ]; then $(RM) $(BUILD_DIR); fi
	@$(MKDIR) $(BUILD_DIR)
	@touch $(TAG_DEBUG)
	$(MAKE_TARGET)

clean:
	$(RM) $(BUILD_DIR)

run: default
	clear
	$(TARGET)

check: test _clear_console
	$(EXPECT) $(TARGET) $(TEST_DIR)/test.rc

unit:
	@cd $(UNIT_TEST_DIR) && $(MAKE) -s run > /dev/null || true

_clear_console:
	clear

# === Build Rules ===

$(TARGET): $(PRECOMP) $(OBJECTS)
	$(CXX) -o $@ $(LDFLAGS) $(OBJECTS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(PRECOMP)
	$(CXX) -c -o $@ $< -MMD -MP -include $(HEADER_SRC) -Winvalid-pch $(CXXFLAGS)

$(PRECOMP): $(HEADER_SRC) | $(BUILD_DIR)
	$(CXX) -o $@ $< -MD $(CXXFLAGS)

$(BUILD_DIR): Makefile
	@$(RM) $@
	@$(MKDIR) $@

-include $(DEPS)
