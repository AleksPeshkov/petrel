# Simple Makefile for unit tests in tests/unit/
BUILD_DIR ?= ./build
TARGET ?= $(BUILD_DIR)/test

# Compiler tracking
COMPILER_STAMP := $(BUILD_DIR)/.compiler-stamp

RM := rm -rf
MKDIR := mkdir -p

# Force CXX to clang++ unless user explicitly sets it
ifeq ($(origin CXX), command line)
	# Keep user choice
else
	override CXX := clang++
	#override CXX := g++
endif

# Compiler and flags
CXXFLAGS := -O3 -std=c++20 -fno-exceptions -fno-rtti -march=native -mtune=native
CXXFLAGS += -mavx2

ifeq ($(CXX), clang++)
	CXXFLAGS += -fconstexpr-steps=10000000
else ifeq ($(CXX), g++)
	CXXFLAGS += -flax-vector-conversions -Wno-class-memaccess -Wno-packed-bitfield-compat -Wno-invalid-constexpr
endif

# Source files
SRC_SOURCES  = $(wildcard *.cpp)

# Object files (preserve structure)
TEST_OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(TEST_SOURCES:.cpp=.o)))
SRC_OBJECTS  = $(addprefix $(BUILD_DIR)/, $(notdir $(SRC_SOURCES:.cpp=.o)))

OBJECTS = $(TEST_OBJECTS) $(SRC_OBJECTS)
DEPS = $(OBJECTS:.o=.d)

.PHONY: all run clean FORCE

all: $(BUILD_DIR) $(TARGET) run

run: $(TARGET)
	@echo "Running zobrist..."
	@./$(TARGET) || (echo "failed!"; exit 1)

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
