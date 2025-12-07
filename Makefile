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

# MPS library (optional)
ifdef MPS_DIR
    CXXFLAGS += -I$(MPS_DIR)/src
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

# Executable
EXECUTABLE := $(BIN_DIR)/lynx_db

# Test files
TEST_SRCS := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJS := $(patsubst $(TEST_DIR)/%.cpp,$(OBJ_DIR)/tests/%.o,$(TEST_SRCS))
TEST_EXECUTABLE := $(BIN_DIR)/lynx_tests

# Google Test library
GTEST_OBJ := $(OBJ_DIR)/gtest/gtest-all.o
GTEST_LIB := $(LIB_OUT_DIR)/libgtest.a

# Default target
.PHONY: all
all: release

# Release build
.PHONY: release
release: BUILD_TYPE=release
release: $(EXECUTABLE) $(LIB_STATIC) $(LIB_SHARED)
	@echo "Build complete (release)"

# Debug build
.PHONY: debug
debug: BUILD_TYPE=debug
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: $(EXECUTABLE) $(LIB_STATIC) $(LIB_SHARED)
	@echo "Build complete (debug)"

# Create directories
$(BIN_DIR) $(OBJ_DIR)/lib $(OBJ_DIR)/tests $(OBJ_DIR)/gtest $(LIB_OUT_DIR):
	@mkdir -p $@

# Compile library objects
$(OBJ_DIR)/lib/%.o: $(LIB_DIR)/%.cpp | $(OBJ_DIR)/lib
	@echo "Compiling $<"
	@$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

# Create obj directory
$(OBJ_DIR):
	@mkdir -p $@

# Compile main object
$(MAIN_OBJ): $(MAIN_SRC) | $(OBJ_DIR)
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

# Executable
$(EXECUTABLE): $(MAIN_OBJ) $(LIB_STATIC) | $(BIN_DIR)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< -L$(LIB_OUT_DIR) -l$(LIB_NAME) $(LDFLAGS)

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

# Run executable
.PHONY: run
run: $(EXECUTABLE)
	@$(EXECUTABLE)

# Build Google Test library
$(GTEST_OBJ): | $(OBJ_DIR)/gtest
	@echo "Building Google Test library"
	@$(CXX) $(CXXFLAGS) -I$(GTEST_DIR)/googletest -I$(GTEST_INC) -c $(GTEST_ALL) -o $@

$(GTEST_LIB): $(GTEST_OBJ) | $(LIB_OUT_DIR)
	@echo "Creating Google Test static library"
	@ar rcs $@ $^

# Compile test objects
$(OBJ_DIR)/tests/%.o: $(TEST_DIR)/%.cpp | $(OBJ_DIR)/tests
	@echo "Compiling test $<"
	@$(CXX) $(CXXFLAGS) -I$(GTEST_INC) -c $< -o $@

# Link test executable
$(TEST_EXECUTABLE): $(TEST_OBJS) $(GTEST_LIB) $(LIB_STATIC) | $(BIN_DIR)
	@echo "Linking test executable $@"
	@$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJS) -L$(LIB_OUT_DIR) -lgtest -l$(LIB_NAME) $(LDFLAGS)

# Build tests
.PHONY: build-tests
build-tests: $(TEST_EXECUTABLE)
	@echo "Tests built successfully"

# Run tests
.PHONY: test
test: $(TEST_EXECUTABLE)
	@echo "Running tests..."
	@$(TEST_EXECUTABLE)

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
	@echo "  make             - Build release"
	@echo "  make debug       - Build debug"
	@echo "  make clean       - Clean build"
	@echo "  make run         - Build and run"
	@echo "  make build-tests - Build unit tests"
	@echo "  make test        - Build and run tests"
	@echo "  make install     - Install to system"
	@echo "  make cleanall    - Clean build and external deps"
	@echo "  make info        - Show this info"

# Help
.PHONY: help
help: info
