# Lynx Vector Database - Makefile
#
# Usage:
#   make          - Build release
#   make debug    - Build debug
#   make clean    - Clean build artifacts
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
EXTERNAL_DIR := external

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

COMPARE_SRC := $(SRC_DIR)/compare_indices.cpp
COMPARE_OBJ := $(OBJ_DIR)/compare_indices.o

# Executables
EXECUTABLE := $(BIN_DIR)/lynx_db
EXECUTABLE_MINIMAL := $(BIN_DIR)/lynx_minimal
EXECUTABLE_COMPARE := $(BIN_DIR)/compare_indices

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
release: $(EXECUTABLE) $(EXECUTABLE_MINIMAL) $(EXECUTABLE_COMPARE) $(LIB_STATIC) $(LIB_SHARED)
	@echo "Build complete (release)"

# Debug build
.PHONY: debug
debug: BUILD_TYPE=debug
debug: CXXFLAGS += -g -O0 -DDEBUG
debug: $(EXECUTABLE) $(EXECUTABLE_MINIMAL) $(EXECUTABLE_COMPARE) $(LIB_STATIC) $(LIB_SHARED)
	@echo "Build complete (debug)"

# Create directories
$(BIN_DIR) $(OBJ_DIR)/lib $(OBJ_DIR)/mps $(LIB_OUT_DIR):
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

$(COMPARE_OBJ): $(COMPARE_SRC) | $(OBJ_DIR)
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

$(EXECUTABLE_COMPARE): $(COMPARE_OBJ) $(LIB_STATIC) $(MPS_LIB) | $(BIN_DIR)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< -L$(LIB_OUT_DIR) -l$(LIB_NAME) -lmps $(LDFLAGS)
else
$(EXECUTABLE): $(MAIN_OBJ) $(LIB_STATIC) | $(BIN_DIR)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< -L$(LIB_OUT_DIR) -l$(LIB_NAME) $(LDFLAGS)

$(EXECUTABLE_MINIMAL): $(MAIN_MINIMAL_OBJ) $(LIB_STATIC) | $(BIN_DIR)
	@echo "Linking $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< -L$(LIB_OUT_DIR) -l$(LIB_NAME) $(LDFLAGS)

$(EXECUTABLE_COMPARE): $(COMPARE_OBJ) $(LIB_STATIC) | $(BIN_DIR)
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

.PHONY: run-compare
run-compare: $(EXECUTABLE_COMPARE)
	@LD_LIBRARY_PATH=$(LIB_OUT_DIR):$$LD_LIBRARY_PATH $(EXECUTABLE_COMPARE)

# Build MPS library
ifdef MPS_DIR
$(MPS_OBJ): $(MPS_SRC) | $(OBJ_DIR)/mps
	@echo "Building MPS library"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(MPS_LIB): $(MPS_OBJ) | $(LIB_OUT_DIR)
	@echo "Creating MPS static library"
	@ar rcs $@ $^
endif

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
	@echo "  make run-compare  - Build and run index comparison"
	@echo "  make install      - Install to system"
	@echo "  make cleanall     - Clean build and external deps"
	@echo "  make info         - Show this info"
	@echo ""
	@echo "For unit tests and code coverage, use: ./setup.sh test"

# Help
.PHONY: help
help: info
