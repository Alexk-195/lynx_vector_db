# GenerateCoverage.cmake
# Script to generate code coverage reports
# Supports lcov (preferred), gcov, and llvm-cov

cmake_minimum_required(VERSION 3.20)

# Get parameters from command line
if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR not defined")
endif()

if(NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "BINARY_DIR not defined")
endif()

message(STATUS "Generating coverage report...")
message(STATUS "  Source directory: ${SOURCE_DIR}")
message(STATUS "  Binary directory: ${BINARY_DIR}")

# First, run the tests to generate coverage data
message(STATUS "Running tests with coverage...")
execute_process(
    COMMAND ${CMAKE_COMMAND} --build ${BINARY_DIR} --target lynx_tests
    RESULT_VARIABLE build_result
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT build_result EQUAL 0)
    message(WARNING "Test build had issues, continuing anyway...")
endif()

# Run tests with proper library path
execute_process(
    COMMAND ${BINARY_DIR}/bin/lynx_tests
    WORKING_DIRECTORY ${BINARY_DIR}
    RESULT_VARIABLE test_result
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error
)

if(test_result EQUAL 0)
    message(STATUS "Tests passed")
else()
    message(WARNING "Some tests failed, but generating coverage report anyway")
    message(STATUS "${test_output}")
endif()

# Try to find coverage tools
find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)
find_program(GCOV_PATH gcov)
find_program(LLVM_COV_PATH llvm-cov)
find_program(LLVM_PROFDATA_PATH llvm-profdata)

# Strategy 1: Try lcov (preferred for GCC)
if(LCOV_PATH AND GENHTML_PATH)
    message(STATUS "Using lcov for coverage reporting...")

    # Capture coverage data
    execute_process(
        COMMAND ${LCOV_PATH} --capture
            --directory ${BINARY_DIR}
            --output-file ${SOURCE_DIR}/coverage.info
            --quiet
            --rc geninfo_unexecuted_blocks=0
            --ignore-errors mismatch,empty
        RESULT_VARIABLE lcov_capture_result
        OUTPUT_VARIABLE lcov_output
        ERROR_VARIABLE lcov_error
    )

    if(NOT lcov_capture_result EQUAL 0)
        message(WARNING "lcov capture had issues: ${lcov_error}")
    endif()

    # Remove unwanted files from coverage
    execute_process(
        COMMAND ${LCOV_PATH} --remove ${SOURCE_DIR}/coverage.info
            '/usr/*' '*/external/*' '*/tests/*'
            --output-file ${SOURCE_DIR}/coverage.info
            --quiet
            --rc geninfo_unexecuted_blocks=0
            --ignore-errors empty
        RESULT_VARIABLE lcov_remove_result
    )

    # List coverage summary
    execute_process(
        COMMAND ${LCOV_PATH} --list ${SOURCE_DIR}/coverage.info
            --ignore-errors empty
        OUTPUT_VARIABLE coverage_summary
    )
    message(STATUS "${coverage_summary}")

    # Generate HTML report
    execute_process(
        COMMAND ${GENHTML_PATH} ${SOURCE_DIR}/coverage.info
            --output-directory ${SOURCE_DIR}/coverage_report
            --quiet
            --ignore-errors empty
        RESULT_VARIABLE genhtml_result
    )

    if(genhtml_result EQUAL 0)
        message(STATUS "")
        message(STATUS "Coverage report generated in coverage_report/index.html")
    else()
        message(WARNING "HTML generation had issues")
    endif()

# Strategy 2: Try gcov (basic GCC coverage)
elseif(GCOV_PATH)
    message(STATUS "Using gcov for coverage reporting...")

    # Find .gcda files
    file(GLOB_RECURSE GCDA_FILES "${BINARY_DIR}/*.gcda")

    if(GCDA_FILES)
        message(STATUS "")
        message(STATUS "Coverage Summary for Library Files:")
        message(STATUS "====================================")

        # Process each .gcda file in the library directory
        file(GLOB LIB_GCDA_FILES "${BINARY_DIR}/CMakeFiles/lynx_static.dir/src/lib/*.gcda")

        foreach(gcda_file ${LIB_GCDA_FILES})
            get_filename_component(gcda_name ${gcda_file} NAME)
            execute_process(
                COMMAND ${GCOV_PATH} ${gcda_file}
                WORKING_DIRECTORY ${BINARY_DIR}/CMakeFiles/lynx_static.dir/src/lib
                OUTPUT_VARIABLE gcov_output
                ERROR_QUIET
            )

            # Extract and display relevant lines
            string(REGEX MATCH "File.*src/lib[^\n]*" file_line "${gcov_output}")
            string(REGEX MATCH "Lines executed:[^\n]*" lines_line "${gcov_output}")

            if(file_line AND lines_line)
                message(STATUS "${file_line}")
                message(STATUS "${lines_line}")
            endif()
        endforeach()

        message(STATUS "")
        message(STATUS "NOTE: Install lcov for HTML reports: sudo apt-get install lcov")
        message(STATUS "Detailed coverage files (.gcov) are in ${BINARY_DIR}/CMakeFiles/lynx_static.dir/src/lib/")
    else()
        message(WARNING "No .gcda files found. Coverage instrumentation may not be enabled.")
        message(STATUS "Make sure to build with -DLYNX_ENABLE_COVERAGE=ON")
    endif()

# Strategy 3: Try llvm-cov (for Clang)
elseif(LLVM_COV_PATH AND LLVM_PROFDATA_PATH)
    message(STATUS "Using llvm-cov for coverage reporting...")

    # Find .profraw files
    file(GLOB PROFRAW_FILES "${BINARY_DIR}/*.profraw")

    if(PROFRAW_FILES)
        # Merge profraw files
        execute_process(
            COMMAND ${LLVM_PROFDATA_PATH} merge -sparse
                ${PROFRAW_FILES}
                -o ${BINARY_DIR}/lynx.profdata
            RESULT_VARIABLE profdata_result
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(profdata_result EQUAL 0 AND EXISTS "${BINARY_DIR}/lynx.profdata")
            # Generate HTML report
            execute_process(
                COMMAND ${LLVM_COV_PATH} show
                    ${BINARY_DIR}/bin/lynx_tests
                    -instr-profile=${BINARY_DIR}/lynx.profdata
                    -format=html
                    -output-dir=${SOURCE_DIR}/coverage_report
                    -ignore-filename-regex=(tests|external)
                RESULT_VARIABLE llvm_cov_result
            )

            if(llvm_cov_result EQUAL 0)
                message(STATUS "")
                message(STATUS "Coverage report generated in coverage_report/index.html")
            else()
                message(WARNING "llvm-cov show failed")
            endif()
        else()
            message(WARNING "Failed to merge profraw files")
        endif()
    else()
        message(WARNING "No .profraw files found. Clang instrumentation may not be enabled.")
    endif()

# No coverage tools found
else()
    message(WARNING "No coverage tools found.")
    message(STATUS "Install one of the following:")
    message(STATUS "  - lcov (for GCC): sudo apt-get install lcov")
    message(STATUS "  - llvm tools (for Clang): sudo apt-get install llvm")
    message(FATAL_ERROR "Cannot generate coverage report without coverage tools")
endif()
