![Build Status](https://github.com/Alexk-195/lynx_vector_db/actions/workflows/c-cpp.yml/badge.svg)

# Lynx Vector Database

A fast and light weight vector database implemented in modern C++20 with support for Approximate Nearest Neighbor (ANN) search algorithms. Suitable for medium size projects on embedded systems. 

## Features

- **Unified Database Architecture**:
  - Single `VectorDatabase` class works with all index types
  - Clean, simple API via `IVectorDatabase::create(config)`
  - Consistent threading model using `std::shared_mutex`
- **Multiple Index Types** (all through unified VectorDatabase):
  - **Flat**: Brute-force exact search - best for <1K vectors, 100% recall
  - **HNSW**: Hierarchical Navigable Small World graphs - O(log N), 95-99% recall
  - **IVF**: Inverted File Index - memory-efficient, fast construction, 85-98% recall
- **Thread-Safe Operations**:
  - **Concurrent reads**: Multiple threads can search simultaneously
  - **Exclusive writes**: Inserts/removes are properly serialized
  - **Advanced option**: `VectorDatabase_MPS` for extreme concurrency (100+ queries)
- **Modern C++20**: Concepts, spans, smart pointers, RAII
- **Flexible Distance Metrics**: L2 (Euclidean), Cosine, Dot Product
- **Persistence**: Save and load database to/from disk

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

# Quality Assurance and Validation (Ticket #2072)
./setup.sh coverage    # Generate code coverage report
./setup.sh tsan        # Run ThreadSanitizer (data race detection)
./setup.sh asan        # Run AddressSanitizer (memory error detection)
./setup.sh ubsan       # Run UndefinedBehaviorSanitizer + AddressSanitizer
./setup.sh valgrind    # Run Valgrind memory leak check (requires valgrind)
./setup.sh clang-tidy  # Run clang-tidy static analysis (requires clang-tidy)
./setup.sh benchmark   # Run performance benchmarks only
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

### Choosing a Database Implementation

Lynx provides two implementations of `IVectorDatabase`:

| Implementation | Threading Model | Best For | Complexity |
|----------------|----------------|----------|------------|
| **VectorDatabase** (Default) | `std::shared_mutex` | Most use cases, embedded systems, < 50 concurrent queries | Low |
| **VectorDatabase_MPS** (Advanced) | Message passing (MPS) | Very high concurrency (100+ queries), non-blocking maintenance, strict latency SLAs | High |

**Quick Start**:
```cpp
// Default (recommended for most use cases)
Config config;
config.index_type = IndexType::HNSW;
auto db = std::make_shared<VectorDatabase>(config);

// Advanced (for extreme performance requirements)
auto db = std::make_shared<VectorDatabase_MPS>(config);
```

**When to use VectorDatabase_MPS**:
- ✅ 100+ concurrent queries
- ✅ Non-blocking index maintenance required
- ✅ Multi-core system (8+ cores)
- ✅ Production systems with high load

**When to use VectorDatabase** (default):
- ✅ Low to medium concurrency (< 50 queries)
- ✅ Small to medium datasets (< 1M vectors)
- ✅ Embedded systems
- ✅ Development and prototyping

See [doc/MPS_ARCHITECTURE.md](doc/MPS_ARCHITECTURE.md) for detailed comparison and migration guide.

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

## Code Quality and Validation Tools

Lynx provides comprehensive validation tools through `setup.sh` for ensuring code quality, thread safety, memory safety, and performance. These tools are essential for production-ready code and are part of the quality assurance process documented in **Ticket #2072**.

### Available Validation Tools

| Tool | Command | Purpose | When to Use |
|------|---------|---------|-------------|
| **Code Coverage** | `./setup.sh coverage` | Measure test coverage, identify untested code paths | Before releases, to ensure comprehensive testing |
| **ThreadSanitizer** | `./setup.sh tsan` | Detect data races and threading issues | After threading changes, before production |
| **AddressSanitizer** | `./setup.sh asan` | Detect memory errors (buffer overflows, use-after-free) | During development, before commits |
| **UBSanitizer** | `./setup.sh ubsan` | Detect undefined behavior + memory errors | For comprehensive safety checks |
| **Valgrind** | `./setup.sh valgrind` | Thorough memory leak detection | Final validation, investigating memory issues |
| **clang-tidy** | `./setup.sh clang-tidy` | Static code analysis, best practices | Code review, quality improvements |
| **Benchmarks** | `./setup.sh benchmark` | Performance testing | After optimizations, regression testing |

### Validation Tool Details

**Code Coverage** (`./setup.sh coverage`):
- Generates HTML coverage report in `build-coverage/coverage_report/`
- Target: >85% overall, >95% on critical paths
- Results saved to `tickets/2072_coverage_report.txt`

**ThreadSanitizer** (`./setup.sh tsan`):
- Detects data races, deadlocks, and thread safety issues
- Essential for multi-threaded code validation
- Results saved to `tickets/2072_tsan_results.txt`
- ⚠️ Note: Slower execution, ~2-5x overhead

**AddressSanitizer** (`./setup.sh asan`):
- Finds buffer overflows, use-after-free, memory corruption
- Fast execution, low overhead (~2x)
- Results saved to `tickets/2072_asan_results.txt`
- Recommended for regular development use

**UndefinedBehaviorSanitizer** (`./setup.sh ubsan`):
- Combines AddressSanitizer + undefined behavior detection
- Catches integer overflows, null pointer dereferences, etc.
- Results saved to `tickets/2072_ubsan_results.txt`
- Most comprehensive sanitizer check

**Valgrind** (`./setup.sh valgrind`):
- Definitive memory leak detection
- Slower execution (~10-50x overhead)
- Results saved to `tickets/2072_valgrind_results.txt`
- Requires: `sudo apt-get install valgrind` (Ubuntu/Debian)

**clang-tidy** (`./setup.sh clang-tidy`):
- Static analysis for code quality
- Enforces modern C++ best practices
- Results saved to `tickets/2072_clang_tidy_results.txt`
- Requires: `sudo apt-get install clang-tidy` (Ubuntu/Debian)

**Benchmarks** (`./setup.sh benchmark`):
- Runs performance benchmark tests only
- Measures search latency, memory usage, throughput
- Results saved to `tickets/2072_benchmark_results.txt`
- Uses Release build for accurate performance metrics

### Recommended Validation Workflow

```bash
# 1. Quick validation (during development)
./setup.sh asan        # Check for memory errors

# 2. Comprehensive validation (before commits)
./setup.sh coverage    # Ensure good test coverage
./setup.sh tsan        # Check threading safety
./setup.sh asan        # Check memory safety

# 3. Release validation (before production)
./setup.sh coverage    # Final coverage check
./setup.sh tsan        # Thread safety validation
./setup.sh ubsan       # Comprehensive sanitizer check
./setup.sh benchmark   # Performance validation
./setup.sh clang-tidy  # Code quality review

# 4. Deep investigation (if issues found)
./setup.sh valgrind    # Thorough memory leak check
```

### Quality Targets

Based on **Ticket #2070** quality assurance standards:

- ✅ Code Coverage: >85% overall, >95% on critical paths
- ✅ ThreadSanitizer: Clean (no race conditions)
- ✅ AddressSanitizer: Clean (no memory errors)
- ✅ UBSanitizer: Clean (no undefined behavior)
- ✅ Valgrind: Zero memory leaks in application code
- ✅ clang-tidy: No critical warnings
- ✅ Benchmarks: No performance regression

### Output Files

All validation tools save their results to the `tickets/` directory:
- `tickets/2072_coverage_report.txt` - Coverage analysis
- `tickets/2072_tsan_results.txt` - ThreadSanitizer output
- `tickets/2072_asan_results.txt` - AddressSanitizer output
- `tickets/2072_ubsan_results.txt` - UBSanitizer output
- `tickets/2072_valgrind_results.txt` - Valgrind output
- `tickets/2072_clang_tidy_results.txt` - Static analysis
- `tickets/2072_benchmark_results.txt` - Performance benchmarks

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

Lynx uses a clean, layered architecture with a unified database implementation:

### Layers

1. **API Layer**: Pure virtual interface (`IVectorDatabase`) in [src/include/lynx/lynx.h](src/include/lynx/lynx.h)
   - Provides factory method: `IVectorDatabase::create(config)`
   - All operations (insert, search, remove, persistence)
   - Thread-safe by design

2. **Database Layer**: Unified `VectorDatabase` implementation
   - Single class that works with all index types
   - Thread-safe using `std::shared_mutex`:
     - Concurrent reads (multiple searches simultaneously)
     - Exclusive writes (inserts/removes serialized)
   - Manages vector storage and metadata
   - Delegates search to the appropriate index

3. **Index Layer**: Pluggable index implementations
   - `FlatIndex` - Brute-force exact search (O(N))
   - `HNSWIndex` - Hierarchical graphs for fast ANN (O(log N))
   - `IVFIndex` - Inverted file with k-means clustering

### Architecture Diagram

```
┌─────────────────────────────────────────────────────────┐
│                  User Application                        │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│              IVectorDatabase (Interface)                 │
│  - create()     - insert()    - search()                 │
│  - remove()     - batch_insert()                         │
│  - get()        - save()/load()                          │
└────────────────────┬────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────┐
│           VectorDatabase (Unified Implementation)        │
│                                                           │
│  ┌─────────────────────────────────────────────────┐    │
│  │  Thread Safety: std::shared_mutex               │    │
│  │  - Concurrent reads (search, get, contains)     │    │
│  │  - Exclusive writes (insert, remove)            │    │
│  └─────────────────────────────────────────────────┘    │
│                                                           │
│  ┌─────────────────────────────────────────────────┐    │
│  │  Vector Storage                                  │    │
│  │  - std::unordered_map<id, VectorRecord>         │    │
│  │  - Metadata management                           │    │
│  └─────────────────────────────────────────────────┘    │
│                                                           │
│                        │                                  │
│                        ▼                                  │
│  ┌──────────────────────────────────────────────────┐   │
│  │         Index Selection (via Config)             │   │
│  │  - IndexType::Flat   → FlatIndex                 │   │
│  │  - IndexType::HNSW   → HNSWIndex                 │   │
│  │  - IndexType::IVF    → IVFIndex                  │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────┬───────────────────────────────────┘
                      │
        ┌─────────────┼─────────────┐
        ▼             ▼             ▼
┌─────────────┐ ┌───────────┐ ┌──────────┐
│  FlatIndex  │ │ HNSWIndex │ │ IVFIndex │
├─────────────┤ ├───────────┤ ├──────────┤
│ Exact       │ │ Graph-    │ │ Cluster- │
│ search      │ │ based ANN │ │ based ANN│
│ O(N)        │ │ O(log N)  │ │ O(k)     │
└─────────────┘ └───────────┘ └──────────┘
```

### Threading Model

All operations are thread-safe using `std::shared_mutex`:

```cpp
// Multiple threads can search concurrently
std::thread t1([&]() { db->search(query1, k); });
std::thread t2([&]() { db->search(query2, k); });  // Concurrent with t1

// Writes are exclusive
std::thread t3([&]() { db->insert(record); });     // Blocks during write

t1.join(); t2.join(); t3.join();
```

**Performance characteristics**:
- Low to medium concurrency (< 50 queries): Excellent performance
- High concurrency (100+ queries): Consider `VectorDatabase_MPS` (see below)
- Embedded systems: Low overhead, simple threading model

### Advanced: VectorDatabase_MPS

For extreme performance requirements, Lynx provides `VectorDatabase_MPS`:
- Message-passing architecture with dedicated thread pools
- Non-blocking index optimization
- Best for: 100+ concurrent queries, strict latency SLAs

See [doc/MPS_ARCHITECTURE.md](doc/MPS_ARCHITECTURE.md) for detailed comparison and when to use it.

## Documentation

- [doc/MIGRATION_GUIDE.md](doc/MIGRATION_GUIDE.md) - Migration guide for unified VectorDatabase
- [doc/MPS_ARCHITECTURE.md](doc/MPS_ARCHITECTURE.md) - MPS infrastructure and when to use it
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

