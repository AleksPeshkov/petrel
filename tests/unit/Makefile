# Simple Makefile for unit tests in tests/unit/
BUILD_DIR ?= ../../build/unit
TARGET = $(BUILD_DIR)/test

RM = rm -rf
MKDIR = mkdir -p

# Compiler and flags
CXX = clang++
CXXFLAGS = -MMD -MP -std=c++20 -Wall -Wextra -g -I../../src -O0 -ggdb -DDEBUG -fsanitize=address,undefined
CXXFLAGS += -mavx2

# Source files
TEST_SOURCES = $(wildcard *.cpp)
SRC_SOURCES  = $(wildcard ../../src/*.cpp)
SRC_SOURCES := $(filter-out ../../src/main.cpp, $(SRC_SOURCES))

# Object files (preserve structure)
TEST_OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(TEST_SOURCES:.cpp=.o)))
SRC_OBJECTS  = $(addprefix $(BUILD_DIR)/, $(notdir $(SRC_SOURCES:.cpp=.o)))

OBJECTS = $(TEST_OBJECTS) $(SRC_OBJECTS)
DEPS = $(OBJECTS:.o=.d)

.PHONY: all run clean

all: $(BUILD_DIR) $(TARGET) run

# Run the test
run: $(TARGET)
	@echo "Running tests..."
	@./$(TARGET) || (echo "Tests failed!"; exit 1)

# Clean up
clean:
	$(RM) $(BUILD_DIR)

# Compile test .cpp files
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile src .cpp files (from ../../src)
$(BUILD_DIR)/%.o: ../../src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link final binary
$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(CXXFLAGS)

# Ensure build directory exists
$(BUILD_DIR): Makefile
	$(MKDIR) $@

# Include dependency files
-include $(DEPS)
