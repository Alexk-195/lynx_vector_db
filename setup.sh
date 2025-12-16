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
#   ./setup.sh coverage     # Build with coverage and generate report
#   ./setup.sh tsan         # Run ThreadSanitizer (data race detection)
#   ./setup.sh asan         # Run AddressSanitizer (memory error detection)
#   ./setup.sh ubsan        # Run UndefinedBehaviorSanitizer + AddressSanitizer
#   ./setup.sh valgrind     # Run Valgrind (memory leak detection)
#   ./setup.sh clang-tidy   # Run clang-tidy static analysis
#   ./setup.sh benchmark    # Run performance benchmarks only
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
        log_info "Building and running tests with CMake..."

        # Create build directory for tests
        mkdir -p build-test
        cd build-test

        # Configure with CMake
        log_info "Configuring CMake..."
        cmake -DCMAKE_BUILD_TYPE=Release \
              -DLYNX_BUILD_TESTS=ON \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              ..

        # Build the project and tests
        log_info "Building project and tests..."
        cmake --build . -j$(nproc 2>/dev/null || echo 4)

        # Run tests with CTest
        log_info "Running tests..."
        ctest --output-on-failure

        cd ..
        log_info "Test run complete"
        ;;
    coverage)
        check_dependencies
        log_info "Building with coverage instrumentation..."

        # Clean previous coverage build
        rm -rf build-coverage coverage_report coverage.info

        # Create coverage build directory
        mkdir -p build-coverage
        cd build-coverage

        # Configure with coverage enabled
        log_info "Configuring CMake with coverage..."
        cmake -DCMAKE_BUILD_TYPE=Debug \
              -DLYNX_ENABLE_COVERAGE=ON \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              ..

        # Build the project
        log_info "Building project with coverage..."
        cmake --build . -j$(nproc 2>/dev/null || echo 4)

        # Run tests with coverage
        log_info "Running tests with coverage..."
        ctest --output-on-failure || log_warn "Some tests failed"

        # Generate coverage report
        log_info "Generating coverage report..."
        cmake --build . --target coverage-report

        cd ..
        log_info "Coverage build complete"
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
    tsan)
        check_dependencies
        log_info "Building with ThreadSanitizer..."

        # Create build directory for ThreadSanitizer
        mkdir -p build-tsan
        cd build-tsan

        # Configure with ThreadSanitizer flags
        log_info "Configuring CMake with ThreadSanitizer..."
        cmake -DCMAKE_BUILD_TYPE=Debug \
              -DCMAKE_CXX_FLAGS="-fsanitize=thread -g" \
              -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
              -DLYNX_BUILD_TESTS=ON \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              ..

        # Build the project
        log_info "Building project with ThreadSanitizer..."
        cmake --build . -j$(nproc 2>/dev/null || echo 4)

        # Run tests with ThreadSanitizer
        log_info "Running tests with ThreadSanitizer..."
        OUTPUT_FILE="${PROJECT_ROOT}/tickets/2072_tsan_results.txt"
        log_info "Output will be saved to ${OUTPUT_FILE}"

        ./bin/lynx_tests 2>&1 | tee "${OUTPUT_FILE}" || log_warn "Some tests failed or warnings detected"

        cd ..
        log_info "ThreadSanitizer check complete. Results saved to ${OUTPUT_FILE}"
        ;;
    asan)
        check_dependencies
        log_info "Building with AddressSanitizer..."

        # Create build directory for AddressSanitizer
        mkdir -p build-asan
        cd build-asan

        # Configure with AddressSanitizer flags
        log_info "Configuring CMake with AddressSanitizer..."
        cmake -DCMAKE_BUILD_TYPE=Debug \
              -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
              -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
              -DLYNX_BUILD_TESTS=ON \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              ..

        # Build the project
        log_info "Building project with AddressSanitizer..."
        cmake --build . -j$(nproc 2>/dev/null || echo 4)

        # Run tests with AddressSanitizer
        log_info "Running tests with AddressSanitizer..."
        OUTPUT_FILE="${PROJECT_ROOT}/tickets/2072_asan_results.txt"
        log_info "Output will be saved to ${OUTPUT_FILE}"

        ./bin/lynx_tests 2>&1 | tee "${OUTPUT_FILE}" || log_warn "Some tests failed or warnings detected"

        cd ..
        log_info "AddressSanitizer check complete. Results saved to ${OUTPUT_FILE}"
        ;;
    ubsan)
        check_dependencies
        log_info "Building with UndefinedBehaviorSanitizer + AddressSanitizer..."

        # Create build directory for UBSan
        mkdir -p build-ubsan
        cd build-ubsan

        # Configure with UBSan + ASan flags
        log_info "Configuring CMake with UndefinedBehaviorSanitizer..."
        cmake -DCMAKE_BUILD_TYPE=Debug \
              -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -g" \
              -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
              -DLYNX_BUILD_TESTS=ON \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              ..

        # Build the project
        log_info "Building project with UndefinedBehaviorSanitizer..."
        cmake --build . -j$(nproc 2>/dev/null || echo 4)

        # Run tests with UBSan
        log_info "Running tests with UndefinedBehaviorSanitizer..."
        OUTPUT_FILE="${PROJECT_ROOT}/tickets/2072_ubsan_results.txt"
        log_info "Output will be saved to ${OUTPUT_FILE}"

        ./bin/lynx_tests 2>&1 | tee "${OUTPUT_FILE}" || log_warn "Some tests failed or warnings detected"

        cd ..
        log_info "UndefinedBehaviorSanitizer check complete. Results saved to ${OUTPUT_FILE}"
        ;;
    valgrind)
        check_dependencies

        # Check if Valgrind is installed
        if ! command -v valgrind &> /dev/null; then
            log_error "Valgrind is not installed. Please install it first:"
            log_error "  Ubuntu/Debian: sudo apt-get install valgrind"
            log_error "  Fedora/RHEL:   sudo dnf install valgrind"
            exit 1
        fi

        log_info "Running Valgrind memory leak check..."

        # Use existing test build or create new one
        if [ ! -d "build-test" ]; then
            log_info "Test build not found, creating it..."
            mkdir -p build-test
            cd build-test
            cmake -DCMAKE_BUILD_TYPE=Debug \
                  -DLYNX_BUILD_TESTS=ON \
                  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
                  ..
            cmake --build . -j$(nproc 2>/dev/null || echo 4)
        else
            cd build-test
        fi

        # Run tests with Valgrind
        log_info "Running tests with Valgrind (this may take a while)..."
        OUTPUT_FILE="${PROJECT_ROOT}/tickets/2072_valgrind_results.txt"
        log_info "Output will be saved to ${OUTPUT_FILE}"

        valgrind --leak-check=full \
                 --show-leak-kinds=all \
                 --track-origins=yes \
                 --verbose \
                 --log-file="${OUTPUT_FILE}" \
                 ./bin/lynx_tests 2>&1 || log_warn "Some tests failed"

        cd ..
        log_info "Valgrind check complete. Results saved to ${OUTPUT_FILE}"
        ;;
    clang-tidy)
        check_dependencies

        # Check if clang-tidy is installed
        if ! command -v clang-tidy &> /dev/null; then
            log_error "clang-tidy is not installed. Please install it first:"
            log_error "  Ubuntu/Debian: sudo apt-get install clang-tidy"
            log_error "  Fedora/RHEL:   sudo dnf install clang-tools-extra"
            exit 1
        fi

        log_info "Running clang-tidy static analysis..."

        # Create build directory with compile commands
        mkdir -p build-clang-tidy
        cd build-clang-tidy

        # Configure to generate compile_commands.json
        log_info "Configuring CMake with compile commands export..."
        cmake -DCMAKE_BUILD_TYPE=Debug \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
              -DLYNX_BUILD_TESTS=ON \
              ..

        # Build the project
        log_info "Building project..."
        cmake --build . -j$(nproc 2>/dev/null || echo 4)

        # Run clang-tidy on all source files
        log_info "Running clang-tidy analysis..."
        OUTPUT_FILE="${PROJECT_ROOT}/tickets/2072_clang_tidy_results.txt"
        log_info "Output will be saved to ${OUTPUT_FILE}"

        # Run clang-tidy on lib files
        echo "=== clang-tidy analysis ===" > "${OUTPUT_FILE}"
        echo "Date: $(date)" >> "${OUTPUT_FILE}"
        echo "" >> "${OUTPUT_FILE}"

        for file in ../src/lib/*.cpp; do
            echo "Analyzing $(basename $file)..." | tee -a "${OUTPUT_FILE}"
            clang-tidy "$file" -p . -- -I../src/include -std=c++20 2>&1 | tee -a "${OUTPUT_FILE}" || true
        done

        cd ..
        log_info "clang-tidy analysis complete. Results saved to ${OUTPUT_FILE}"
        ;;
    benchmark)
        check_dependencies
        log_info "Running performance benchmarks..."

        # Use existing test build or create new one
        if [ ! -d "build-test" ]; then
            log_info "Test build not found, creating it..."
            mkdir -p build-test
            cd build-test
            cmake -DCMAKE_BUILD_TYPE=Release \
                  -DLYNX_BUILD_TESTS=ON \
                  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
                  ..
            cmake --build . -j$(nproc 2>/dev/null || echo 4)
        else
            cd build-test
        fi

        # Run benchmark tests only
        log_info "Running benchmark tests..."
        OUTPUT_FILE="${PROJECT_ROOT}/tickets/2072_benchmark_results.txt"
        log_info "Output will be saved to ${OUTPUT_FILE}"

        ./bin/lynx_tests --gtest_filter="*Benchmark*" 2>&1 | tee "${OUTPUT_FILE}" || log_warn "Some benchmarks failed"

        cd ..
        log_info "Benchmark run complete. Results saved to ${OUTPUT_FILE}"
        ;;
    *)
        echo "Usage: $0 {release|debug|clean|rebuild|install|test|coverage|tsan|asan|ubsan|valgrind|clang-tidy|benchmark|run|info}"
        exit 1
        ;;
esac

exit 0
