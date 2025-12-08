#!/bin/bash
#
# Lynx Vector Database - Build Setup Script
#
# Usage:
#   ./setup.sh              # Build release
#   ./setup.sh debug        # Build debug
#   ./setup.sh clean        # Clean build directory
#   ./setup.sh rebuild      # Clean and rebuild
#   ./setup.sh test         # Run tests
#   ./setup.sh install      # Install to system (requires sudo)
#

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

setup_mps() {
    log_info "Checking for MPS library..."

    # Check for MPS_PATH environment variable first
    if [ -n "${MPS_PATH}" ] && [ -d "${MPS_PATH}" ]; then
        export MPS_DIR="${MPS_PATH}"
        log_info "Using MPS from MPS_PATH: ${MPS_PATH}"
        return 0
    fi

    # Check for MPS_DIR environment variable
    if [ -n "${MPS_DIR}" ] && [ -d "${MPS_DIR}" ]; then
        log_info "Using MPS from MPS_DIR: ${MPS_DIR}"
        return 0
    fi

    # Check if MPS exists in external/mps
    local MPS_LOCAL="${PROJECT_ROOT}/external/mps"
    if [ -d "${MPS_LOCAL}" ]; then
        export MPS_DIR="${MPS_LOCAL}"
        log_info "Using MPS from external/mps"
        return 0
    fi

    # Auto-clone MPS to external/mps
    log_warn "MPS not found. Cloning from repository..."
    mkdir -p "${PROJECT_ROOT}/external"

    if git clone https://github.com/Alexk-195/mps.git "${MPS_LOCAL}"; then
        export MPS_DIR="${MPS_LOCAL}"
        log_info "MPS successfully cloned to ${MPS_LOCAL}"
    else
        log_error "Failed to clone MPS repository"
        log_error "You can manually set MPS_PATH or MPS_DIR environment variable"
        exit 1
    fi
}

setup_googletest() {
    log_info "Checking for Google Test..."

    local GTEST_PATH="${PROJECT_ROOT}/external/googletest"

    if [ -d "${GTEST_PATH}" ]; then
        log_info "Google Test found in external/googletest"
        return 0
    fi

    # Auto-clone Google Test
    log_warn "Google Test not found. Cloning from repository..."
    mkdir -p "${PROJECT_ROOT}/external"

    if git clone --depth 1 --branch v1.15.2 https://github.com/google/googletest.git "${GTEST_PATH}"; then
        log_info "Google Test successfully cloned to ${GTEST_PATH}"
    else
        log_error "Failed to clone Google Test repository"
        log_error "CMake will attempt to fetch it automatically during build"
    fi
}

check_dependencies() {
    log_info "Checking dependencies..."

    # Setup MPS library
    setup_mps

    # Setup Google Test library
    setup_googletest

    # Check compiler
    if command -v g++ &> /dev/null; then
        gxx_version=$(g++ --version | head -n1)
        log_info "Compiler: ${gxx_version}"
    elif command -v clang++ &> /dev/null; then
        clang_version=$(clang++ --version | head -n1)
        log_info "Compiler: ${clang_version}"
    else
        log_error "No C++ compiler found (g++ or clang++)"
        exit 1
    fi

    # Check make
    if ! command -v make &> /dev/null; then
        log_error "make is required but not installed."
        exit 1
    fi
}

# Main script logic
cd "${PROJECT_ROOT}"

case "${1}" in
    clean)
        log_info "Cleaning build directory..."
        make clean
        log_info "Clean complete."
        ;;
    rebuild)
        log_info "Rebuilding project..."
        make clean
        check_dependencies
        make release
        ;;
    install)
        check_dependencies
        make release
        make install
        ;;
    test)
        check_dependencies
        make release
        make test
        ;;
    debug)
        check_dependencies
        log_info "Building debug version..."
        make debug
        ;;
    release|"")
        check_dependencies
        log_info "Building release version..."
        make release
        ;;
    run)
        check_dependencies
        make release
        make run
        ;;
    info)
        make info
        ;;
    *)
        echo "Usage: $0 {release|debug|clean|rebuild|install|test|run|info}"
        exit 1
        ;;
esac

exit 0
