#!/bin/bash

# Build and run Lynx benchmark

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

echo "Building Lynx benchmark..."

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found. Please run './setup.sh' first."
    exit 1
fi

# Compile the benchmark
g++ -std=c++20 -O3 \
    -I"$PROJECT_ROOT/src/include" \
    "$SCRIPT_DIR/lynx_test.cpp" \
    -L"$BUILD_DIR/lib" \
    -llynx \
    -pthread \
    -o "$SCRIPT_DIR/lynx_test" \
    -Wl,-rpath,"$BUILD_DIR/lib"

if [ $? -ne 0 ]; then
    echo "Error: Compilation failed."
    exit 1
fi

echo "Running Lynx benchmark..."
echo "=========================="
"$SCRIPT_DIR/lynx_test"
