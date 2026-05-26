# Simple Makefile for unit tests in tests/unit/
BUILD_DIR ?= ../../build/unit
TARGET ?= $(BUILD_DIR)/test

# Compiler tracking
COMPILER_STAMP := $(BUILD_DIR)/.compiler-stamp

RM := rm -rf
MKDIR := mkdir -p

# Compiler and flags
CXX ?= clang++
CXXFLAGS := -MMD -MP -std=c++20 -Wall -Wextra -g -I../../src -O0 -ggdb -DDEBUG -fsanitize=address,undefined
CXXFLAGS += -mavx2

ifeq ($(CXX), clang++)
	CXXFLAGS += -fconstexpr-steps=10000000
else ifeq ($(CXX), g++)
	CXXFLAGS += -flax-vector-conversions -Wno-class-memaccess
endif

# Source files
TEST_SOURCES = $(wildcard *.cpp)
SRC_SOURCES  = $(wildcard ../../src/*.cpp)
SRC_SOURCES := $(filter-out ../../src/main.cpp, $(SRC_SOURCES))

# Object files (preserve structure)
TEST_OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(TEST_SOURCES:.cpp=.o)))
SRC_OBJECTS  = $(addprefix $(BUILD_DIR)/, $(notdir $(SRC_SOURCES:.cpp=.o)))

OBJECTS = $(TEST_OBJECTS) $(SRC_OBJECTS)
DEPS = $(OBJECTS:.o=.d)

.PHONY: all run clean FORCE

all: $(BUILD_DIR) $(TARGET) run

run: $(TARGET)
	@echo "Running tests..."
	@./$(TARGET) || (echo "Tests failed!"; exit 1)

clean:
	$(RM) $(BUILD_DIR)

FORCE:

# Compiler change detection
$(COMPILER_STAMP): FORCE | $(BUILD_DIR)
	@prev=""; \
	if [ -f "$@" ]; then prev="$$(cat '$@' 2>/dev/null || echo '')"; fi; \
	curr="$(CXX)"; \
	if [ "x$$curr" != "x$$prev" ]; then \
		echo "🔄 Compiler changed: $$prev → $$curr, rebuilding..."; \
		$(RM) $(OBJECTS) $(TARGET); \
		echo "$$curr" > '$@'; \
	else \
		echo "✅ Compiler unchanged: $$curr"; \
	fi

# Compile test .cpp files
$(BUILD_DIR)/%.o: %.cpp $(COMPILER_STAMP)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile src .cpp files (from ../../src)
$(BUILD_DIR)/%.o: ../../src/%.cpp $(COMPILER_STAMP)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link final binary
$(TARGET): $(OBJECTS) | $(COMPILER_STAMP)
	$(CXX) -o $@ $^ $(CXXFLAGS)

# Ensure build directory exists
$(BUILD_DIR): Makefile
	@$(RM) $@
	@$(MKDIR) $@

# Include dependency files
-include $(DEPS)
