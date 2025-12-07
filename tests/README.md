# Lynx Vector Database - Unit Tests

This directory contains the unit test suite for the Lynx vector database, using Google Test (gtest) framework.

## Test Structure

- `test_main.cpp` - Main test runner entry point
- `test_utility_functions.cpp` - Tests for utility functions (error_string, index_type_string, etc.)
- `test_config.cpp` - Tests for configuration structures and default values
- `test_data_structures.cpp` - Tests for data structures (SearchResult, VectorRecord, etc.)
- `test_database.cpp` - Tests for VectorDatabase interface and operations

## Running Tests

### Using Make
```bash
make test              # Build and run all tests
make build-tests       # Build tests without running
```

### Using setup.sh
```bash
./setup.sh test        # Build and run all tests
```

### Direct Execution
```bash
./build/bin/lynx_tests # Run the test executable directly
```

## Test Coverage

Current test coverage (67 tests):

### Utility Functions (17 tests)
- ✓ Error code string representations
- ✓ Index type string representations
- ✓ Distance metric string representations
- ✓ Version string

### Configuration (24 tests)
- ✓ Config default values
- ✓ HNSWParams default values
- ✓ IVFParams default values
- ✓ SearchParams default values
- ✓ Configuration customization

### Data Structures (7 tests)
- ✓ SearchResultItem construction
- ✓ SearchResult creation and population
- ✓ VectorRecord with/without metadata
- ✓ DatabaseStats initialization and values

### Database Operations (17 tests)
- ✓ Database creation with various configs
- ✓ Database properties (size, dimension, stats)
- ✓ Insert operations (currently returns NotImplemented)
- ✓ Search operations (currently returns empty results)
- ✓ Remove operations (currently returns NotImplemented)
- ✓ Batch operations (currently returns NotImplemented)
- ✓ Persistence operations (currently returns NotImplemented)

### Future Test Categories

As implementation progresses, additional tests will be added for:

- Distance metric calculations (L2, Cosine, Dot Product)
- HNSW index operations
- Thread safety and concurrency
- Persistence and serialization
- Memory management
- Performance benchmarks

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

This project uses **Google Test v1.15.2**, located in `external/googletest/`.

### Resources

- [Google Test Documentation](https://google.github.io/googletest/)
- [Primer](https://google.github.io/googletest/primer.html)
- [Advanced Guide](https://google.github.io/googletest/advanced.html)

## Dependencies

The test suite automatically builds and links against:
- Google Test library (built from source)
- Lynx static library (`liblynx.a`)

No system-wide installation of Google Test is required.

## Continuous Integration

Tests should be run before:
- Committing changes
- Creating pull requests
- Releasing new versions

All tests must pass for the build to be considered successful.
