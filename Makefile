# Lynx Vector Database - Makefile
#
# Usage:
#   make          - Build release
#   make debug    - Build debug
#   make clean    - Clean build artifacts
#   make test     - Run tests
#   make install  - Install to system

# Compiler settings
CXX := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Werror=return-type
LDFLAGS := -pthread

# Directories
SRC_DIR := src
LIB_DIR := $(SRC_DIR)/lib
INC_DIR := $(SRC_DIR)/include
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
OBJ_DIR := $(BUILD_DIR)/obj
LIB_OUT_DIR := $(BUILD_DIR)/lib
TEST_DIR := tests
EXTERNAL_DIR := external
GTEST_DIR := $(EXTERNAL_DIR)/googletest

# MPS library
ifdef MPS_DIR
    MPS_INC_DIR := $(MPS_DIR)/src
    MPS_SRC_DIR := $(MPS_DIR)/src
else ifneq (,$(wildcard $(EXTERNAL_DIR)/mps))
    # Use external/mps if it exists
    MPS_INC_DIR := $(EXTERNAL_DIR)/mps/src
    MPS_SRC_DIR := $(EXTERNAL_DIR)/mps/src
endif

# Add MPS include path if available
ifdef MPS_INC_DIR
    CXXFLAGS += -I$(MPS_INC_DIR)
endif

# Include paths
CXXFLAGS += -I$(INC_DIR)

# Google Test paths
GTEST_INC := $(GTEST_DIR)/googletest/include
GTEST_SRC := $(GTEST_DIR)/googletest/src
GTEST_ALL := $(GTEST_SRC)/gtest-all.cc
GTEST_MAIN := $(GTEST_SRC)/gtest_main.cc

# Build type (default: release)
BUILD_TYPE ?= release

ifeq ($(BUILD_TYPE),debug)
    CXXFLAGS += -g -O0 -DDEBUG
else
    CXXFLAGS += -O3 -DNDEBUG
endif

# Library settings
LIB_NAME := lynx
LIB_STATIC := $(LIB_OUT_DIR)/lib$(LIB_NAME).a
LIB_SHARED := $(LIB_OUT_DIR)/lib$(LIB_NAME).so

# Source files
LIB_SRCS := $(wildcard $(LIB_DIR)/*.cpp)
LIB_OBJS := $(patsubst $(LIB_DIR)/%.cpp,$(OBJ_DIR)/lib/%.o,$(LIB_SRCS))

MAIN_SRC := $(SRC_DIR)/main.cpp
MAIN_OBJ := $(OBJ_DIR)/main.o

MAIN_MINIMAL_SRC := $(SRC_DIR)/main_minimal.cpp
MAIN_MINIMAL_OBJ := $(OBJ_DIR)/main_minimal.o

# Executables
EXECUTABLE := $(BIN_DIR)/lynx_db
EXECUTABLE_MINIMAL := $(BIN_DIR)/lynx_minimal

# Test files
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))
TEST_EXECUTABLE := $(BIN_DIR)/lynx_tests

# Google Test library
GTEST_OBJ := $(OBJ_DIR)/gtest/gtest-all.o
GTEST_LIB := $(LIB_OUT_DIR)/libgtest.a

# MPS library sources
ifdef MPS_SRC_DIR
    MPS_SRC := $(MPS_SRC_DIR)/mps.cpp
    MPS_OBJ := $(OBJ_DIR)/mps/mps.o
    MPS_LIB := $(LIB_OUT_DIR)/libmps.a
else
    MPS_SRC :=
    MPS_OBJ :=
    MPS_LIB :=
endif

# Default target
.PHONY: all
all: release

# Release build
.PHONY: release
release: BUILD_TYPE=release
release: $(EXECUTABLE) $(EXECUTABLE_MINIMAL) $(LIB_STATIC) $(LIB_SHARED)
	@echo "Build complete (release)"

# Debug build
.PHONY: debug
debug: BUILD_TYPE=debug
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: $(EXECUTABLE) $(EXECUTABLE_MINIMAL) $(LIB_STATIC) $(LIB_SHARED)
	@echo "Build complete (debug)"

# Create directories
$(BIN_DIR) $(OBJ_DIR)/lib $(OBJ_DIR)/tests $(OBJ_DIR)/gtest $(OBJ_DIR)/mps $(LIB_OUT_DIR):
	@mkdir -p $@

# Compile library objects
$(OBJ_DIR)/lib/%.o: $(LIB_DIR)/%.cpp | $(OBJ_DIR)/lib
	@echo "Compiling $<"
	@$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

# Create obj directory
$(OBJ_DIR):
	@mkdir -p $@

# Compile main objects
$(MAIN_OBJ): $(MAIN_SRC) | $(OBJ_DIR)
	@echo "Compiling $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(MAIN_MINIMAL_OBJ): $(MAIN_MINIMAL_SRC) | $(OBJ_DIR)
	@echo "Compiling $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Static library
$(LIB_STATIC): $(LIB_OBJS) | $(LIB_OUT_DIR)
	@echo "Creating static library $@"
	@ar rcs $@ $^

# Shared library
$(LIB_SHARED): $(LIB_OBJS) | $(LIB_OUT_DIR)
	@echo "Creating shared library $@"
	@$(CXX) -shared -o $@ $^ $(LDFLAGS)

# Executables
ifdef MPS_LIB
$(EXECUTABLE): $(MAIN_OBJ) $(LIB_STATIC) $(MPS_LIB) | $(BIN_DIR)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< -L$(LIB_OUT_DIR) -l$(LIB_NAME) -lmps $(LDFLAGS)

$(EXECUTABLE_MINIMAL): $(MAIN_MINIMAL_OBJ) $(LIB_STATIC) $(MPS_LIB) | $(BIN_DIR)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< -L$(LIB_OUT_DIR) -l$(LIB_NAME) -lmps $(LDFLAGS)
else
$(EXECUTABLE): $(MAIN_OBJ) $(LIB_STATIC) | $(BIN_DIR)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< -L$(LIB_OUT_DIR) -l$(LIB_NAME) $(LDFLAGS)

$(EXECUTABLE_MINIMAL): $(MAIN_MINIMAL_OBJ) $(LIB_STATIC) | $(BIN_DIR)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< -L$(LIB_OUT_DIR) -l$(LIB_NAME) $(LDFLAGS)
endif

# Clean
.PHONY: clean
clean:
	@echo "Cleaning build artifacts"
	@rm -rf $(BUILD_DIR)

# Clean everything including external dependencies
.PHONY: cleanall
cleanall: clean
	@echo "Cleaning external dependencies"
	@rm -rf $(EXTERNAL_DIR)

# Run executables
.PHONY: run
run: $(EXECUTABLE)
	@LD_LIBRARY_PATH=$(LIB_OUT_DIR):$$LD_LIBRARY_PATH $(EXECUTABLE)

.PHONY: run-minimal
run-minimal: $(EXECUTABLE_MINIMAL)
	@LD_LIBRARY_PATH=$(LIB_OUT_DIR):$$LD_LIBRARY_PATH $(EXECUTABLE_MINIMAL)

# Build Google Test library
$(GTEST_OBJ): | $(OBJ_DIR)/gtest
	@echo "Building Google Test library"
	@$(CXX) $(CXXFLAGS) -I$(GTEST_DIR)/googletest -I$(GTEST_INC) -c $(GTEST_ALL) -o $@

$(GTEST_LIB): $(GTEST_OBJ) | $(LIB_OUT_DIR)
	@echo "Creating Google Test static library"
	@ar rcs $@ $^

# Build MPS library
ifdef MPS_DIR
$(MPS_OBJ): $(MPS_SRC) | $(OBJ_DIR)/mps
	@echo "Building MPS library"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(MPS_LIB): $(MPS_OBJ) | $(LIB_OUT_DIR)
	@echo "Creating MPS static library"
	@ar rcs $@ $^
endif

# Compile test objects
$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.cpp | $(OBJ_DIR)/tests
	@echo "Compiling test $<"
	@$(CXX) $(CXXFLAGS) -I$(GTEST_INC) -c $< -o $@

# Link test executable
ifdef MPS_LIB
$(TEST_EXECUTABLE): $(TEST_OBJS) $(GTEST_LIB) $(MPS_LIB) $(LIB_STATIC) | $(BIN_DIR)
	@echo "Linking test executable $@"
	@$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJS) -L$(LIB_OUT_DIR) -lgtest -l$(LIB_NAME) -lmps $(LDFLAGS)
else
$(TEST_EXECUTABLE): $(TEST_OBJS) $(GTEST_LIB) $(LIB_STATIC) | $(BIN_DIR)
	@echo "Linking test executable $@"
	@$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJS) -L$(LIB_OUT_DIR) -lgtest -l$(LIB_NAME) $(LDFLAGS)
endif

# Build tests
.PHONY: build-tests
build-tests: $(TEST_EXECUTABLE)
	@echo "Tests built successfully"

# Run tests
.PHONY: test
test: $(TEST_EXECUTABLE)
	@echo "Running tests..."
	@LD_LIBRARY_PATH=$(LIB_OUT_DIR):$$LD_LIBRARY_PATH $(TEST_EXECUTABLE)

# Install
.PHONY: install
install: $(LIB_SHARED) $(LIB_STATIC)
	@echo "Installing to /usr/local"
	@sudo mkdir -p /usr/local/include/lynx
	@sudo mkdir -p /usr/local/lib
	@sudo cp $(INC_DIR)/lynx/*.h /usr/local/include/lynx/
	@sudo cp $(LIB_STATIC) $(LIB_SHARED) /usr/local/lib/
	@sudo ldconfig
	@echo "Installation complete"

# Uninstall
.PHONY: uninstall
uninstall:
	@echo "Uninstalling from /usr/local"
	@sudo rm -rf /usr/local/include/lynx
	@sudo rm -f /usr/local/lib/lib$(LIB_NAME).a
	@sudo rm -f /usr/local/lib/lib$(LIB_NAME).so
	@sudo ldconfig
	@echo "Uninstallation complete"

# Show configuration
.PHONY: info
info:
	@echo "Lynx Vector Database Build Configuration"
	@echo "========================================="
	@echo "CXX:        $(CXX)"
	@echo "CXXFLAGS:   $(CXXFLAGS)"
	@echo "LDFLAGS:    $(LDFLAGS)"
	@echo "BUILD_TYPE: $(BUILD_TYPE)"
	@echo "MPS_DIR:    $(MPS_DIR)"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build release"
	@echo "  make debug        - Build debug"
	@echo "  make clean        - Clean build"
	@echo "  make run          - Build and run main example"
	@echo "  make run-minimal  - Build and run minimal example"
	@echo "  make build-tests  - Build unit tests"
	@echo "  make test         - Build and run tests"
	@echo "  make install      - Install to system"
	@echo "  make cleanall     - Clean build and external deps"
	@echo "  make info         - Show this info"
	@echo ""
	@echo "For code coverage, use: ./setup.sh coverage"

# Help
.PHONY: help
help: info
