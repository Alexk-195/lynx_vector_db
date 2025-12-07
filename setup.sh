#!/bin/bash
#
# Lynx Vector Database - Build Setup Script
#
# Usage:
#   ./setup.sh              # Configure and build (Release)
#   ./setup.sh debug        # Configure and build (Debug)
#   ./setup.sh clean        # Clean build directory
#   ./setup.sh rebuild      # Clean and rebuild
#   ./setup.sh install      # Install to system (requires sudo)
#

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
BUILD_TYPE="${1:-Release}"

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

check_dependencies() {
    log_info "Checking dependencies..."

    # Check CMake
    if ! command -v cmake &> /dev/null; then
        log_error "CMake is required but not installed."
        exit 1
    fi
    cmake_version=$(cmake --version | head -n1 | awk '{print $3}')
    log_info "CMake version: ${cmake_version}"

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

    # Check for MPS library
    if [ -z "${MPS_DIR}" ]; then
        log_warn "MPS_DIR not set. Set it to point to the MPS library location."
        log_warn "Example: export MPS_DIR=/path/to/mps"
    else
        log_info "MPS_DIR: ${MPS_DIR}"
    fi
}

clean_build() {
    log_info "Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
    log_info "Clean complete."
}

configure() {
    log_info "Configuring project (${BUILD_TYPE})..."

    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    cmake_args=(
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    )

    if [ -n "${MPS_DIR}" ]; then
        cmake_args+=(-DMPS_DIR="${MPS_DIR}")
    fi

    cmake "${cmake_args[@]}" "${PROJECT_ROOT}"

    # Create symlink for compile_commands.json (for IDE support)
    if [ -f "${BUILD_DIR}/compile_commands.json" ]; then
        ln -sf "${BUILD_DIR}/compile_commands.json" "${PROJECT_ROOT}/compile_commands.json"
    fi

    log_info "Configuration complete."
}

build() {
    log_info "Building project..."

    cd "${BUILD_DIR}"

    # Detect number of CPU cores
    if command -v nproc &> /dev/null; then
        NPROC=$(nproc)
    else
        NPROC=4
    fi

    cmake --build . --parallel "${NPROC}"

    log_info "Build complete."
    log_info "Binaries located in: ${BUILD_DIR}/bin"
    log_info "Libraries located in: ${BUILD_DIR}/lib"
}

install_project() {
    log_info "Installing project..."
    cd "${BUILD_DIR}"
    sudo cmake --install .
    log_info "Installation complete."
}

run_tests() {
    log_info "Running tests..."
    cd "${BUILD_DIR}"
    ctest --output-on-failure
    log_info "Tests complete."
}

# Main script logic
case "${1}" in
    clean)
        clean_build
        ;;
    rebuild)
        clean_build
        check_dependencies
        configure
        build
        ;;
    install)
        if [ ! -d "${BUILD_DIR}" ]; then
            check_dependencies
            configure
            build
        fi
        install_project
        ;;
    test)
        if [ ! -d "${BUILD_DIR}" ]; then
            check_dependencies
            configure
            build
        fi
        run_tests
        ;;
    debug)
        BUILD_TYPE="Debug"
        check_dependencies
        configure
        build
        ;;
    release|"")
        BUILD_TYPE="Release"
        check_dependencies
        configure
        build
        ;;
    *)
        echo "Usage: $0 {release|debug|clean|rebuild|install|test}"
        exit 1
        ;;
esac

exit 0
