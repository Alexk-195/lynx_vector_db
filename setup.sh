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

check_dependencies() {
    log_info "Checking dependencies..."

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

    # Check for MPS library
    if [ -z "${MPS_DIR}" ]; then
        log_warn "MPS_DIR not set. Set it to point to the MPS library location."
        log_warn "Example: export MPS_DIR=/path/to/mps"
    else
        log_info "MPS_DIR: ${MPS_DIR}"
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
