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

# === Tag Files for Build Type ===
TAG_TEST  = $(BUILD_DIR)/.test
TAG_DEBUG = $(BUILD_DIR)/.debug

CXXFLAGS = -std=c++20 -fno-exceptions -fno-rtti
WARNINGS = -Wall -Wpedantic -Wextra

# === Set Flags Based on Tags ===
ifeq ($(wildcard $(TAG_TEST)), $(TAG_TEST))
	BUILD_FLAGS = -Og -ggdb -DDEBUG -DENABLE_ASSERT_LOGGING
	BUILD_FLAGS += -fsanitize=address,undefined
	WARNINGS += -Wno-inline
else ifeq ($(wildcard $(TAG_DEBUG)), $(TAG_DEBUG))
	BUILD_FLAGS = -O0 -ggdb -DDEBUG -DENABLE_ASSERT_LOGGING
	BUILD_FLAGS += -fsanitize=address,undefined
else
	BUILD_FLAGS = -O3 -flto -finline-functions -DNDEBUG
	WARNINGS += -Winline
endif

BUILD_FLAGS += -march=native -mtune=native -static

# === Common Flags ===
WARNINGS += -Wuninitialized -Wcast-qual -Wshadow -Wmissing-declarations -Wstrict-aliasing=1 -Wstrict-overflow=1 -Wsign-promo
WARNINGS += -Wpacked -Wdisabled-optimization -Wredundant-decls -Wextra-semi -Wsuggest-override

ifeq ($(CXX), g++)
	BUILD_FLAGS += -flto=auto --param inline-unit-growth=1000
	CXXFLAGS += -flax-vector-conversions
	WARNINGS += -Wno-class-memaccess -Wno-invalid-constexpr
	WARNINGS += -Wuseless-cast -Wcast-align=strict -Wsuggest-final-types -Wsuggest-final-methods
	WARNINGS += -Wnormalized -Wunsafe-loop-optimizations -Wvector-operation-performance
else ifeq ($(CXX), clang++)
	WARNINGS += -Wcast-align -Wconditional-uninitialized -Wmissing-prototypes -Winvalid-constexpr
	#WARNINGS += -Wconversion
endif

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

CXXFLAGS += $(BUILD_FLAGS) $(WARNINGS)

# === Linker Flags ===
LDLIBS += -pthread
LDFLAGS += $(LDLIBS) $(BUILD_FLAGS)

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))
DEPS = $(patsubst %.o, %.d, $(OBJECTS))

MAKE_TARGET = make --jobs --warn-undefined-variables --no-print-directory $(TARGET)

.PHONY: default release test debug clean run bench check unit _clear_console

default: _clear_console $(BUILD_DIR)
	$(MAKE_TARGET)

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

bench: default
	clear
	$(TARGET) bench

check: test _clear_console
	$(EXPECT) $(TARGET) $(TEST_DIR)/test.rc

unit:
	@cd $(UNIT_TEST_DIR) && $(MAKE) -s run > /dev/null || true

_clear_console:
	clear

# === Build Rules ===

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $(LDFLAGS) $(OBJECTS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) -c -o $@ $< -MMD -MP $(CXXFLAGS)

$(BUILD_DIR): Makefile
	@$(RM) $@
	@$(MKDIR) $@

-include $(DEPS)
