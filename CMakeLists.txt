cmake_minimum_required(VERSION 3.20)
project(lynx_vector_db
        VERSION 0.1.0
        DESCRIPTION "Lynx Vector Database - High-performance ANN search"
        LANGUAGES CXX
)

# C++20 standard required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type defaults to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Compiler warnings
add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
)

# Options
option(LYNX_BUILD_TESTS "Build unit tests" ON)
option(LYNX_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(LYNX_USE_SIMD "Enable SIMD optimizations" ON)
option(LYNX_ENABLE_COVERAGE "Enable code coverage reporting" OFF)

# Code coverage configuration
if(LYNX_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Code coverage not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src/include)

# Find required packages
find_package(Threads REQUIRED)

# MPS library (Message Processing System)
# Check multiple locations in order of priority:
# 1. MPS_PATH environment variable or CMake variable
# 2. MPS_DIR environment variable or CMake variable
# 3. external/mps directory in project root

set(MPS_FOUND FALSE)

# Check MPS_PATH first (environment or CMake variable)
if(DEFINED ENV{MPS_PATH} AND EXISTS "$ENV{MPS_PATH}/src")
    set(MPS_DIR "$ENV{MPS_PATH}")
    set(MPS_FOUND TRUE)
    message(STATUS "Using MPS from MPS_PATH: ${MPS_DIR}")
elseif(DEFINED MPS_PATH AND EXISTS "${MPS_PATH}/src")
    set(MPS_DIR "${MPS_PATH}")
    set(MPS_FOUND TRUE)
    message(STATUS "Using MPS from MPS_PATH: ${MPS_DIR}")
# Check MPS_DIR (environment or CMake variable)
elseif(DEFINED ENV{MPS_DIR} AND EXISTS "$ENV{MPS_DIR}/src")
    set(MPS_DIR "$ENV{MPS_DIR}")
    set(MPS_FOUND TRUE)
    message(STATUS "Using MPS from MPS_DIR: ${MPS_DIR}")
elseif(DEFINED MPS_DIR AND EXISTS "${MPS_DIR}/src")
    set(MPS_FOUND TRUE)
    message(STATUS "Using MPS from MPS_DIR: ${MPS_DIR}")
# Check external/mps directory
elseif(EXISTS "${CMAKE_SOURCE_DIR}/external/mps/src")
    set(MPS_DIR "${CMAKE_SOURCE_DIR}/external/mps")
    set(MPS_FOUND TRUE)
    message(STATUS "Using MPS from external/mps")
endif()

if(MPS_FOUND)
    include_directories(${MPS_DIR}/src)

    # Build MPS as a static library
    add_library(mps_static STATIC ${MPS_DIR}/src/mps.cpp)
    target_include_directories(mps_static PUBLIC ${MPS_DIR}/src)
    target_link_libraries(mps_static PUBLIC Threads::Threads)
    set_target_properties(mps_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    message(FATAL_ERROR
        "MPS library not found!\n"
        "Please either:\n"
        "  1. Set MPS_PATH environment variable: export MPS_PATH=/path/to/mps\n"
        "  2. Set MPS_DIR environment variable: export MPS_DIR=/path/to/mps\n"
        "  3. Run ./setup.sh to automatically clone MPS to external/mps\n"
        "  4. Manually clone MPS: git clone https://github.com/Alexk-195/mps.git external/mps"
    )
endif()

# Lynx static library
add_library(lynx_static STATIC
        src/lib/lynx.cpp
        src/lib/vector_database_impl.cpp
        src/lib/vector_database_mps.cpp
        src/lib/hnsw_index.cpp
)

target_include_directories(lynx_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(lynx_static PUBLIC
        Threads::Threads
        mps_static
)

set_target_properties(lynx_static PROPERTIES
        OUTPUT_NAME lynx
        PUBLIC_HEADER "src/include/lynx/lynx.h"
)

# Lynx shared library
add_library(lynx SHARED
        src/lib/lynx.cpp
        src/lib/vector_database_impl.cpp
        src/lib/vector_database_mps.cpp
        src/lib/hnsw_index.cpp
)

target_include_directories(lynx PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(lynx PUBLIC
        Threads::Threads
        mps_static
)

set_target_properties(lynx PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        PUBLIC_HEADER "src/include/lynx/lynx.h"
)

# Main executable
add_executable(lynx_db
        src/main.cpp
)

target_link_libraries(lynx_db PRIVATE
        lynx
)

# Installation
include(GNUInstallDirs)

install(TARGETS lynx lynx_static lynx_db
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lynx
)

install(DIRECTORY src/include/lynx
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Testing
if(LYNX_BUILD_TESTS)
    enable_testing()

    # Fetch Google Test
    include(FetchContent)

    # Use local googletest if available, otherwise fetch it
    if(EXISTS ${CMAKE_SOURCE_DIR}/external/googletest)
        message(STATUS "Using local Google Test from external/googletest")
        add_subdirectory(${CMAKE_SOURCE_DIR}/external/googletest EXCLUDE_FROM_ALL)
    else()
        message(STATUS "Fetching Google Test from GitHub")
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            GIT_SHALLOW TRUE
        )

        # Prevent overriding the parent project's compiler/linker settings on Windows
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

        FetchContent_MakeAvailable(googletest)
    endif()

    # Add test executable
    add_executable(lynx_tests
        tests/test_main.cpp
        tests/test_utility_functions.cpp
        tests/test_config.cpp
        tests/test_data_structures.cpp
        tests/test_database.cpp
        tests/test_distance_metrics.cpp
        tests/test_hnsw.cpp
    )

    target_link_libraries(lynx_tests PRIVATE
        lynx_static
        gtest
        gtest_main
        Threads::Threads
    )

    target_include_directories(lynx_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
    )

    # Add tests to CTest
    include(GoogleTest)
    gtest_discover_tests(lynx_tests)

    # Add custom test target for convenience
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS lynx_tests
        COMMENT "Running tests..."
    )

    message(STATUS "Tests enabled - use 'make test' or 'make check' to run")
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "Lynx Vector Database Configuration:")
message(STATUS "  Version:          ${PROJECT_VERSION}")
message(STATUS "  Build type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS "  Install prefix:   ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build tests:      ${LYNX_BUILD_TESTS}")
message(STATUS "  Build benchmarks: ${LYNX_BUILD_BENCHMARKS}")
message(STATUS "  Use SIMD:         ${LYNX_USE_SIMD}")
message(STATUS "  Code coverage:    ${LYNX_ENABLE_COVERAGE}")
message(STATUS "")
