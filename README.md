![Build Status](https://github.com/Alexk-195/lynx_vector_db/actions/workflows/c-cpp.yml/badge.svg)

# Lynx Vector Database

A fast and light weight vector database implemented in modern C++20 with support for Approximate Nearest Neighbor (ANN) search algorithms. Suitable for medium size projects on embedded systems. 

## Features

- **Multiple Index Types**:
  - **HNSW**: Hierarchical Navigable Small World graphs for O(log N) query time and high recall
  - **IVF**: Inverted File Index for fast construction and memory-efficient approximate search
  - **Flat**: Brute-force exact search for small datasets
- **Multi-threaded**: Built on MPS (Message Processing System) for concurrent operations
- **Modern C++20**: Concepts, spans, ranges, and coroutines
- **Flexible Distance Metrics**: L2 (Euclidean), Cosine, Dot Product
- **Persistence**: Save and load indices to/from disk

## Requirements

- C++20 compatible compiler (GCC 11+, Clang 14+)
- GNU Make or CMake 3.20+
- Git (for automatic dependency cloning)

### Dependencies (Auto-managed)

The following dependencies are **automatically handled** by the build scripts:

- **MPS library** ([github.com/Alexk-195/mps](https://github.com/Alexk-195/mps))
  - Checked in order: `MPS_PATH` env var → `MPS_DIR` env var → `external/mps` → auto-clone
- **Google Test v1.15.2** ([github.com/google/googletest](https://github.com/google/googletest))
  - Automatically cloned to `external/googletest` or fetched by CMake

## Quick Start

### Clone and Build

```bash
git clone https://github.com/Alexk-195/lynx_vector_db.git
cd lynx_vector_db

# Build with setup.sh (recommended - handles all dependencies automatically)
./setup.sh

# OR build with CMake (dependencies auto-detected or fetched)
mkdir build && cd build
cmake ..
cmake --build .
```

### Custom Dependency Locations

If you have MPS installed elsewhere, set the environment variable:

```bash
# Option 1: Use MPS_PATH
export MPS_PATH=/path/to/mps
./setup.sh

# Option 2: Use MPS_DIR (legacy)
export MPS_DIR=/path/to/mps
./setup.sh

# Option 3: Let setup.sh auto-clone to external/mps (no env var needed)
./setup.sh
```

### Build Options - setup.sh (Makefile)

```bash
./setup.sh          # Build release version
./setup.sh debug    # Build debug version
./setup.sh clean    # Clean build artifacts
./setup.sh rebuild  # Clean and rebuild
./setup.sh test     # Run unit tests
./setup.sh install  # Install to system (requires sudo)
./setup.sh coverage  # Do test coverage
```

### Build Options - Make

```bash
make              # Build release
make debug        # Build debug
make clean        # Clean build
make run          # Build and run
make info         # Show build configuration

# Note: MPS_DIR is automatically set by setup.sh
# If using make directly, set MPS_PATH or MPS_DIR manually
export MPS_PATH=/path/to/mps
make

# Note: To run tests, use ./setup.sh test or CMake (see Testing section below)
```

### Build Options - CMake

```bash
# Configure (dependencies auto-detected)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# With custom MPS location (if needed)
cmake .. -DMPS_PATH=/path/to/mps
# OR
cmake .. -DMPS_DIR=/path/to/mps

# Build
cmake --build . --parallel

# Run tests (see Testing section below for more options)
ctest                    # Run all tests
make test                # Alternative way to run all tests
make check               # Run tests with verbose output

# Install
sudo cmake --install .
```

## Usage

For examples see [src/main_minimal.cpp](src/main_minimal.cpp) and [src/main.cpp](src/main.cpp) and [src/compare_indices.cpp](src/compare_indices.cpp)

## Testing

### Running Tests

#### Quick Start

```bash
# Using setup.sh (recommended - automatically configures CMake)
./setup.sh test

# Or using CMake directly
cd build
ctest                 # Run all tests
```

#### All Tests

```bash
cd build

# Run all tests
ctest

# Run all tests with output
ctest --output-on-failure

# Run all tests with verbose output
ctest -V

# Run all tests quietly
ctest -Q
```

#### Running Specific Tests

```bash
cd build

# Run tests matching a pattern (regex)
ctest -R "ThreadingTest"                    # All threading tests
ctest -R "IVFDatabaseTest"                  # All IVF database tests
ctest -R "HNSW"                             # All HNSW-related tests

# Run specific test by exact name
ctest -R "^IVFDatabaseTest.BatchInsertAndSearch$"

# Run multiple patterns
ctest -R "Threading|IVF"                    # Threading OR IVF tests

# Exclude tests matching a pattern
ctest -E "Benchmark"                        # Exclude benchmark tests

# Combine include and exclude
ctest -R "Threading" -E "Stress"            # Threading tests except stress tests
```

#### Running Tests with Verbose Output

```bash
cd build

# Verbose output for specific tests
ctest -R "ThreadingTest" -V

# Show only failures
ctest -R "IVFDatabaseTest" --output-on-failure

# Rerun only failed tests
ctest --rerun-failed

# Stop on first failure
ctest --stop-on-failure
```

#### Running Tests Multiple Times

```bash
cd build

# Run tests multiple times to check for flakiness
ctest -R "StatisticsConsistency" --repeat until-pass:5

# Run a specific test 10 times
for i in {1..10}; do
    echo "=== Run $i ==="
    ctest -R "ConcurrentWrites" --output-on-failure || break
done
```

#### Test Categories

| Category | Pattern | Description |
|----------|---------|-------------|
| **Threading Tests** | `ctest -R "ThreadingTest"` | Multi-threaded safety tests |
| **IVF Tests** | `ctest -R "IVFDatabaseTest\|IVFIndexTest"` | IVF index tests |
| **HNSW Tests** | `ctest -R "HNSWTest\|HNSWDatabaseTest"` | HNSW index tests |
| **Flat Tests** | `ctest -R "FlatIndexTest"` | Flat/brute-force index tests |
| **Distance Tests** | `ctest -R "DistanceMetricsTest"` | Distance metric tests |
| **Persistence Tests** | `ctest -R "Persistence"` | Save/load functionality tests |
| **Benchmark Tests** | `ctest -R "Benchmark"` | Performance benchmarks |

#### Using Google Test Filters (Advanced)

```bash
cd build

# Run the test binary directly with Google Test filters
./bin/lynx_tests --gtest_filter="ThreadingTest.*"
./bin/lynx_tests --gtest_filter="*IVF*"
./bin/lynx_tests --gtest_list_tests                    # List all tests
./bin/lynx_tests --gtest_filter="ThreadingTest.*" --gtest_repeat=10
```

### Choosing an Index Type

| Index Type | Best For | Query Speed | Memory | Construction | Recall |
|------------|----------|-------------|---------|--------------|---------|
| **Flat** | Small datasets (<1K), exact search required | Slow (O(N)) | Low | Instant | 100% |
| **HNSW** | High-performance ANN, low latency critical | Fast (O(log N)) | High | Slow | 95-99% |
| **IVF** | Memory-constrained, fast construction needed | Medium | Low | Fast | 85-98% |

### IVF Parameter Tuning

**n_clusters** (number of clusters):
- Rule of thumb: `sqrt(N)` where N is the number of vectors
- Example: For 10,000 vectors, use ~100 clusters
- Larger values: Faster search but may require higher n_probe for good recall
- Smaller values: Slower search but better recall with low n_probe

**n_probe** (clusters to search):
- Controls the speed/recall tradeoff
- Start with 10 and adjust based on your needs:
  - n_probe=1: Fastest queries, ~60-70% recall
  - n_probe=10: Balanced, ~90-95% recall
  - n_probe=sqrt(n_clusters): High recall, ~98%+
- Higher values = better recall but slower queries

**Example tuning process**:
```cpp
// Start with default configuration
config.ivf_params.n_clusters = 100;  // For ~10K vectors
config.ivf_params.n_probe = 10;

// If recall is too low, increase n_probe
params.n_probe = 20;  // Better recall, slower

// If queries are too slow, decrease n_probe
params.n_probe = 5;   // Faster, lower recall

// For datasets with natural clusters, fewer clusters may work better
config.ivf_params.n_clusters = 50;  // Try fewer clusters
```

## Project Structure

```
lynx_vector_db/
├── Makefile             # Build configuration
├── setup.sh             # Build script
├── CLAUDE.md            # AI assistant instructions
├── README.md            # This file
├── LICENSE              # MIT License
├── doc/
│   └── research.md      # ANN algorithm research and other files
├── tickets/
│   ├── README.md        # Ticketing system documentation
│   └── done/            # Completed tickets
├── src/
│   ├── include/
│   │   └── lynx/
│   │       └── lynx.h   # Public interface
│   ├── lib/
│   │   └── ...          # Implementation files
│   ├── main_minimal.cpp         # Example of minimal database executable
│   └── main.cpp         # Larger example of database usage
└── tests/
    └── ...              # Unit tests
```

## Architecture

Lynx uses a layered architecture:

1. **API Layer**: Pure virtual interface (`IVectorDatabase`) defined in [src/include/lynx/lynx.h](src/include/lynx/lynx.h)
2. **Index Layer**: Index interface and implementations

## Documentation

- [doc/research.md](doc/research.md) - ANN algorithm research
- [tests/README.md](tests/README.md) - Infos about unit testing
- [tickets/README.md](tickets/README.md) - File-based ticketing system

## Contributing

1. Read [CLAUDE.md](CLAUDE.md) for coding guidelines
2. Review open tickets in [tickets/](tickets/) for tasks to work on
3. Follow the code style conventions
4. Add tests for new functionality

## License

MIT License - see [LICENSE](LICENSE) for details.

