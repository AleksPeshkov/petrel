# Simple Makefile for unit tests in tests/unit/
BUILD_DIR ?= ../../build/unit
TARGET = $(BUILD_DIR)/test

RM = rm -rf
MKDIR = mkdir -p

# Compiler and flags
CXX = clang++
CXXFLAGS = -MMD -MP -std=c++20 -Wall -Wextra -g -I../../src
SOURCES = $(wildcard *.cpp)
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(SOURCES:.cpp=.o)))
DEPS = $(patsubst %.o, %.d, $(OBJECTS))

.PHONY: all run clean

all: $(BUILD_DIR) $(TARGET) run

# Run the test
run: $(TARGET)
	@./$(TARGET) > /dev/null || true

# Clean up
clean:
	$(RM) $(BUILD_DIR)

# Compile .cpp to .o in $(BUILD_DIR)
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Link final test binary
$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(CXXFLAGS)

$(BUILD_DIR): Makefile
	@$(RM) $@
	@$(MKDIR) $@

-include $(DEPS)
