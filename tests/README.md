# Lynx Vector Database - Unit Tests

This directory contains the unit test suite for the Lynx vector database, using Google Test (gtest) framework.

## Test Folder Structure

- `test_main.cpp` - Main test runner entry point
- `README.md` - This file

Actual test implementation is done in other files starting with "test_".

## Running Tests

### Using Make (Makefile)
```bash
make test              # Build and run all tests
make build-tests       # Build tests without running
```

### Using CMake
```bash
# Configure
mkdir build && cd build
cmake ..

# Build
cmake --build .

# Run tests
ctest                  # CTest with individual test discovery
# OR
make test              # CMake's test target
# OR
make check             # Custom target with verbose output
```

### Using setup.sh
```bash
./setup.sh test        # Build and run all tests (uses Makefile)
```

### Direct Execution
```bash
# Makefile build
./build/bin/lynx_tests

# CMake build
./cmake-build/bin/lynx_tests
```

## Test Coverage

Try to achieve overall coverage over 80% and for critical code 100%

## Writing New Tests

### Test File Template

```cpp
/**
 * @file test_feature.cpp
 * @brief Unit tests for Feature
 */

#include <gtest/gtest.h>
#include "lynx/lynx.h"

TEST(FeatureTest, TestCaseName) {
    // Arrange
    // ... setup code ...

    // Act
    // ... execute code under test ...

    // Assert
    EXPECT_EQ(actual, expected);
}
```

### Common Assertions

```cpp
EXPECT_EQ(val1, val2);         // val1 == val2
EXPECT_NE(val1, val2);         // val1 != val2
EXPECT_LT(val1, val2);         // val1 < val2
EXPECT_LE(val1, val2);         // val1 <= val2
EXPECT_GT(val1, val2);         // val1 > val2
EXPECT_GE(val1, val2);         // val1 >= val2

EXPECT_TRUE(condition);        // condition is true
EXPECT_FALSE(condition);       // condition is false

EXPECT_FLOAT_EQ(val1, val2);   // float equality (with tolerance)
EXPECT_DOUBLE_EQ(val1, val2);  // double equality (with tolerance)

EXPECT_STREQ(str1, str2);      // C-string equality
```

### Test Naming Convention

- Test suite names: `ClassNameTest` or `FeatureTest`
- Test case names: `MethodName_StateUnderTest_ExpectedBehavior`
- Examples:
  - `ConfigTest.DefaultDimension`
  - `VectorDatabaseTest.InsertReturnsNotImplemented`
  - `HNSWIndexTest.SearchFindsNearestNeighbors`

## Google Test Framework

This project uses **Google Test v1.15.2**.

### Build System Integration

**Makefile:** Uses local Google Test from `external/googletest/` directory
- Builds gtest from source into `build/lib/libgtest.a`
- Fast compilation, no external downloads

**CMake:** Smart Google Test integration
- First tries to use local `external/googletest/` if available
- Falls back to FetchContent to download from GitHub if not found
- Uses `gtest_discover_tests()` for individual test discovery in CTest

### Resources

- [Google Test Documentation](https://google.github.io/googletest/)
- [Primer](https://google.github.io/googletest/primer.html)
- [Advanced Guide](https://google.github.io/googletest/advanced.html)

## Dependencies

The test suite automatically builds and links against:
- Google Test library (v1.15.2)
- Lynx static library (`liblynx.a` or `lynx_static`)

No system-wide installation of Google Test is required.

## Continuous Integration

Tests should be run before:
- Committing changes
- Creating pull requests
- Releasing new versions

All tests must pass for the build to be considered successful.
