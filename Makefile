# === Configuration ===
EXE := petrel
BUILD_DIR := build
TARGET ?= $(BUILD_DIR)/$(EXE)

SRC_DIR := src
TEST_DIR := tests/integration
UNIT_TEST_DIR := tests/unit

CXX := clang++
#CXX := g++
RM := rm -rf
MKDIR := mkdir -p
CLS := clear

NNUE ?= net/quantised.bin
NNUE_STAMP := $(BUILD_DIR)/$(notdir $(NNUE)).stamp
CHECKSUM ?= sha1sum

# === Tag Files for Build Type ===
TAG_TEST  := $(BUILD_DIR)/tag_test
TAG_DEBUG := $(BUILD_DIR)/tag_debug

# === Common Flags ===
BUILD_ARCH := -march=native -mtune=native
#BUILD_ARCH := -march=x86-64-v3 -mtune=znver3 -static
CXXFLAGS := -std=c++20 -fno-exceptions -fno-rtti
WARNINGS := -Wall -Wpedantic -Wextra
WARNINGS += -Wcast-qual -Wshadow -Wmissing-declarations -Wredundant-decls -Wextra-semi -Wsuggest-override
WARNINGS += -Wuninitialized -Wstrict-aliasing=1 -Wstrict-overflow=1 -Wpacked -Wsign-promo
WARNINGS += -Wdisabled-optimization -Winvalid-constexpr

# === Set Flags Based on Tags ===
ifeq ($(wildcard $(TAG_TEST)), $(TAG_TEST))
	BUILD_FLAGS += -Og -ggdb
else ifeq ($(wildcard $(TAG_DEBUG)), $(TAG_DEBUG))
	BUILD_FLAGS += -O0 -ggdb -fsanitize=address,undefined
else
	BUILD_FLAGS += -O3 -flto
	DEFINES += -DNDEBUG
endif

ifeq ($(CXX), g++)
	BUILD_FLAGS += -flto=auto --param inline-unit-growth=1000
	CXXFLAGS += -flax-vector-conversions
	WARNINGS += -Wno-class-memaccess
	WARNINGS += -Wuseless-cast -Wcast-align=strict -Wsuggest-final-types -Wsuggest-final-methods
	WARNINGS += -Wnormalized -Wunsafe-loop-optimizations -Wvector-operation-performance
else ifeq ($(CXX), clang++)
	WARNINGS += -Winline -Wcast-align -Wconditional-uninitialized -Wmissing-prototypes
endif

GIT_DATE := $(shell git log -1 --date=short --pretty=format:%cd 2>/dev/null || true)
ifneq ($(GIT_DATE),)
	DEFINES += -DGIT_DATE=\"$(GIT_DATE)\"
endif

GIT_SHA := $(shell git log -1 --date=short --pretty=format:%h 2>/dev/null || true)
ifneq ($(GIT_SHA),)
	DEFINES += -DGIT_SHA=\"$(GIT_SHA)\"
endif

GIT_ORIGIN := $(shell git remote get-url origin 2>/dev/null || true)
ifneq ($(GIT_ORIGIN),)
#	DEFINES += -DGIT_ORIGIN=\"$(GIT_ORIGIN)\"
endif

CXXFLAGS := $(BUILD_ARCH) $(BUILD_FLAGS) $(CXXFLAGS) $(WARNINGS) $(DEFINES)
LDFLAGS := $(BUILD_FLAGS)

# === Build Targets ===
MAKE_TARGET := @make --jobs --warn-undefined-variables --no-print-directory $(TARGET)

.PHONY: default release test debug clean run bench perft unit

default: $(BUILD_DIR)
	$(CLS)
	$(MAKE_TARGET)

release: $(BUILD_DIR)
	@if [ -f $(TAG_TEST) ] || [ -f $(TAG_DEBUG) ]; then $(RM) $(BUILD_DIR); fi
	@$(MKDIR) $(BUILD_DIR)
	$(MAKE_TARGET)

test: $(BUILD_DIR)
	@if [ ! -f $(TAG_TEST) ]; then $(RM) $(BUILD_DIR); fi
	@$(MKDIR) $(BUILD_DIR)
	@touch $(TAG_TEST)
	$(MAKE_TARGET)

debug: $(BUILD_DIR)
	@if [ ! -f $(TAG_DEBUG) ]; then $(RM) $(BUILD_DIR); fi
	@$(MKDIR) $(BUILD_DIR)
	@touch $(TAG_DEBUG)
	$(MAKE_TARGET)

clean:
	$(RM) $(BUILD_DIR)

run: default
	$(CLS)
	$(TARGET)

bench: default
	$(CLS)
	$(TARGET) bench

perft: test
	$(CLS)
	$(TEST_DIR)/expect.sh $(TARGET) $(TEST_DIR)/perft.rc

unit:
	@cd $(UNIT_TEST_DIR) && $(MAKE) -s run > /dev/null || true

# === Build Rules ===

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SOURCES))
DEPS := $(patsubst %.o, %.d, $(OBJECTS))

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $(LDFLAGS) $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) -c -o $@ $< -MMD -MP $(CXXFLAGS)

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp $(NNUE_STAMP)

$(NNUE_STAMP): $(BUILD_DIR)
	@prev=$$(cat $(NNUE_STAMP) 2>/dev/null || echo ''); \
	curr=$$($(CHECKSUM) $(NNUE) | cut -d' ' -f1); \
	if [ "$$curr" != "$$prev" ]; then \
		echo "$$curr" > $(NNUE_STAMP); \
	fi

$(BUILD_DIR): Makefile
	@$(RM) $@
	@$(MKDIR) $@

-include $(DEPS)
