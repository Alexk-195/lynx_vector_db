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

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src/include)

# Find required packages
find_package(Threads REQUIRED)

# MPS library (Message Processing System)
# TODO: Add proper find module or fetch content for MPS
# For now, assume MPS is installed in system or specified via MPS_DIR
if(DEFINED MPS_DIR)
    include_directories(${MPS_DIR}/src)
    message(STATUS "MPS directory: ${MPS_DIR}")
else()
    message(WARNING "MPS_DIR not specified. Set -DMPS_DIR=/path/to/mps")
endif()

# Lynx shared library
add_library(lynx SHARED
        src/lib/lynx.cpp  # Placeholder until implementation begins
)

target_include_directories(lynx PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(lynx PUBLIC
        Threads::Threads
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

install(TARGETS lynx lynx_db
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
    # TODO: Add test subdirectory when tests are implemented
    # add_subdirectory(tests)
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
message(STATUS "")
