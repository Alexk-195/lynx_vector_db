#!/bin/bash

# Build and run FAISS benchmark for Lynx Vector Database comparison
# This script compiles faiss_test.cpp and executes the benchmark

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_faiss"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=================================================="
echo "FAISS HNSW Benchmark - Build and Run"
echo "=================================================="
echo ""

# Check if FAISS is installed
echo "Checking for FAISS installation..."
if ! pkg-config --exists faiss 2>/dev/null; then
    if ! ldconfig -p | grep -q libfaiss; then
        echo -e "${RED}ERROR: FAISS library not found!${NC}"
        echo ""
        echo "Please install FAISS first. Options:"
        echo ""
        echo "1. Install via conda (recommended):"
        echo "   conda install -c pytorch faiss-cpu"
        echo "   # or for GPU support:"
        echo "   conda install -c pytorch faiss-gpu"
        echo ""
        echo "2. Build from source:"
        echo "   git clone https://github.com/facebookresearch/faiss.git"
        echo "   cd faiss"
        echo "   cmake -B build -DFAISS_ENABLE_GPU=OFF -DFAISS_ENABLE_PYTHON=OFF ."
        echo "   cmake --build build --parallel"
        echo "   sudo cmake --install build"
        echo ""
        echo "3. Install pre-built package (Ubuntu/Debian):"
        echo "   # Note: May not be available on all distributions"
        echo "   sudo apt-get install libfaiss-dev"
        echo ""
        exit 1
    fi
fi

echo -e "${GREEN}✓ FAISS found${NC}"
echo ""

# Detect FAISS installation paths
FAISS_INCLUDE=""
FAISS_LIB=""

# Try pkg-config first
if pkg-config --exists faiss 2>/dev/null; then
    FAISS_INCLUDE=$(pkg-config --cflags faiss)
    FAISS_LIB=$(pkg-config --libs faiss)
    echo "Using pkg-config for FAISS"
# Try conda installation
elif [ -n "$CONDA_PREFIX" ]; then
    FAISS_INCLUDE="-I$CONDA_PREFIX/include"
    FAISS_LIB="-L$CONDA_PREFIX/lib -lfaiss"
    echo "Using conda FAISS installation: $CONDA_PREFIX"
# Try system installation
else
    FAISS_INCLUDE="-I/usr/local/include -I/usr/include"
    FAISS_LIB="-L/usr/local/lib -L/usr/lib -lfaiss"
    echo "Using system FAISS installation"
fi

echo "FAISS Include: $FAISS_INCLUDE"
echo "FAISS Lib: $FAISS_LIB"
echo ""

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Compile
echo "Compiling faiss_test.cpp..."
g++ -std=c++20 -O3 -march=native \
    $FAISS_INCLUDE \
    -o faiss_test \
    "$SCRIPT_DIR/faiss_test.cpp" \
    $FAISS_LIB \
    -lpthread \
    -fopenmp

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Compilation successful${NC}"
    echo ""
else
    echo -e "${RED}✗ Compilation failed${NC}"
    exit 1
fi

# Run benchmark
echo "=================================================="
echo "Running FAISS HNSW Benchmark"
echo "=================================================="
echo ""

./faiss_test

echo ""
echo "=================================================="
echo "Benchmark Complete"
echo "=================================================="
