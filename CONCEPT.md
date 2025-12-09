# Lynx Vector Database - Design Concept

## Overview

Lynx is a high-performance vector database designed for Approximate Nearest Neighbor (ANN) search. It leverages modern C++20 features and the MPS (Message Processing System) library for multi-threaded operations.
Design was created based on research document in doc/research.md.

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         Client Application                          │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      IVectorDatabase Interface                       │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌───────────────┐  │
│  │   insert()  │ │  search()   │ │  remove()   │ │ batch_insert()│  │
│  └─────────────┘ └─────────────┘ └─────────────┘ └───────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        VectorDatabase (impl)                         │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │                     MPS Threading Layer                        │  │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐    │  │
│  │  │ Query Pool  │  │ Index Pool  │  │  Maintenance Pool   │    │  │
│  │  │  (readers)  │  │  (writers)  │  │   (background)      │    │  │
│  │  └─────────────┘  └─────────────┘  └─────────────────────┘    │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          IVectorIndex Interface                      │
│  ┌─────────────────────────────────────────────────────────────────┐│
│  │  HNSWIndex  │  IVFIndex  │  FlatIndex  │  (future indices)     ││
│  └─────────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         Storage Layer                                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────┐  │
│  │  Vector Store   │  │  Index Store    │  │  Metadata Store     │  │
│  │ (memory-mapped) │  │ (serialization) │  │  (key-value)        │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

## Core Interfaces

### IVectorDatabase
The main entry point for all database operations. Pure virtual interface.

```cpp
class IVectorDatabase {
public:
    virtual ~IVectorDatabase() = default;

    // Single vector operations
    virtual void insert(uint64_t id, std::span<const float> vector) = 0;
    virtual void remove(uint64_t id) = 0;
    virtual bool contains(uint64_t id) const = 0;

    // Search operations
    virtual SearchResult search(std::span<const float> query, size_t k) const = 0;
    virtual SearchResult search(std::span<const float> query, size_t k,
                                const SearchParams& params) const = 0;

    // Batch operations
    virtual void batch_insert(std::span<const VectorRecord> records) = 0;

    // Database info
    virtual size_t size() const = 0;
    virtual size_t dimension() const = 0;
    virtual DatabaseStats stats() const = 0;
};
```

### IVectorIndex
Internal interface for index implementations.

```cpp
class IVectorIndex {
public:
    virtual ~IVectorIndex() = default;

    virtual void add(uint64_t id, std::span<const float> vector) = 0;
    virtual void remove(uint64_t id) = 0;
    virtual std::vector<SearchResultItem> search(
        std::span<const float> query, size_t k, size_t ef_search) const = 0;

    virtual void build(std::span<const VectorRecord> vectors) = 0;
    virtual void serialize(std::ostream& out) const = 0;
    virtual void deserialize(std::istream& in) = 0;
};
```

## MPS Integration

### Worker Types

1. **QueryWorker**: Handles search requests (read-only, high concurrency)
2. **IndexWorker**: Handles insertions and deletions (write, serialized)
3. **MaintenanceWorker**: Background optimization tasks

### Message Types

```cpp
// Base message
struct DatabaseMessage : mps::message {
    uint64_t request_id;
};

// Operations
struct InsertMessage : DatabaseMessage { ... };
struct SearchMessage : DatabaseMessage { ... };
struct BatchInsertMessage : DatabaseMessage { ... };
struct RemoveMessage : DatabaseMessage { ... };
```

### Pool Configuration

- **Query Pool**: `N` workers (N = hardware_concurrency)
- **Index Pool**: 1-2 workers (serialized writes)
- **Maintenance Pool**: 1 worker (background tasks)

## Distance Metrics

| Metric | Formula | Use Case |
|--------|---------|----------|
| L2 (Euclidean) | `sqrt(sum((a[i] - b[i])^2))` | General purpose |
| Cosine | `1 - (a·b)/(|a|*|b|)` | Normalized vectors, text embeddings |
| Dot Product | `-a·b` | When vectors are pre-normalized |

### Distance Function Implementation

All distance metric calculations are centralized in `utils.h` and `utils.cpp` to avoid code duplication:

- **Location**: `src/include/lynx/utils.h` and `src/lib/utils.cpp`
- **Functions**:
  - `utils::calculate_l2_squared()` - Squared L2 distance (faster, no sqrt)
  - `utils::calculate_l2()` - L2 (Euclidean) distance
  - `utils::calculate_cosine()` - Cosine distance
  - `utils::calculate_dot_product()` - Negative dot product
  - `utils::calculate_distance()` - Generic dispatcher based on metric type

These utility functions are used by:
- **Public API** (`lynx.cpp`): Exposes distance functions to library users
- **HNSW Index** (`hnsw_index.cpp`): Internal distance calculations for graph operations
- **Future indices**: Any new index implementations can reuse these optimized functions

## Index Algorithms

### HNSW (Primary)
- Query: O(log N)
- Construction: O(N·D·log N)
- Memory: High (graph overhead)
- Best for: Low latency, high recall requirements

### IVF (Planned)
- Query: O(N/k · n_probe)
- Construction: O(k·D) for k-means
- Memory: Low to moderate
- Best for: Memory-constrained, filtered search

### Flat (Brute Force)
- Query: O(N·D)
- Construction: O(1)
- Memory: O(N·D)
- Best for: Small datasets, validation

## Implementation Phases

### Phase 1: Foundation (Current)
- [x] Project structure and build system
- [x] Interface definitions (pure virtual classes)
- [x] Basic types (Vector, SearchResult, Config)
- [x] Distance metric implementations
- [x] Unit test framework setup

### Phase 2: HNSW Index
- [x] HNSW graph structure
- [x] Insert algorithm with level generation
- [x] Search algorithm (greedy traversal)
- [x] Layer management
- [x] Thread-safe concurrent reads

### Phase 3: MPS Threading
- [x] MPS integration
- [x] Query worker pool
- [x] Index worker pool
- [x] Message routing
- [x] Async API with futures

### Phase 4: Database Layer
- [x] VectorDatabase implementation
- [x] Configuration management
- [x] Statistics collection
- [x] Error handling

### Phase 5: Persistence
- [x] Vector serialization
- [x] Index serialization/deserialization
- [x] Memory-mapped file support
- [x] Filtered search

## Design Decisions

### Why Pure Virtual Interfaces?
1. **Testability**: Easy to mock for unit tests
2. **Extensibility**: New implementations without changing clients
3. **ABI Stability**: Implementation changes don't break binary compatibility
4. **Dependency Injection**: Clean separation of concerns

### Why MPS over std::thread?
1. **Message Passing**: Clean communication between components
2. **Worker Pools**: Built-in thread management
3. **Embedded Focus**: Designed for resource-constrained environments
4. **Synchronization**: Built-in thread-safe primitives

### Why HNSW as Primary Index?
1. **Best Recall/Latency**: O(log N) with 95%+ recall
2. **Incremental Updates**: Supports dynamic inserts
3. **Industry Standard**: Used by Milvus, Qdrant, Weaviate
4. **Well Understood**: Extensive research and benchmarks

## Configuration Options

```cpp
struct Config {
    // Vector configuration
    size_t dimension = 128;
    DistanceMetric distance_metric = DistanceMetric::L2;

    // Index configuration
    IndexType index_type = IndexType::HNSW;

    // HNSW parameters
    size_t hnsw_m = 16;                    // Max connections
    size_t hnsw_ef_construction = 200;     // Build quality
    size_t hnsw_ef_search = 50;            // Search quality

    // Threading
    size_t num_query_threads = 0;          // 0 = auto-detect
    size_t num_index_threads = 2;

    // Storage
    std::string data_path;                 // Empty = in-memory only
    bool enable_wal = false;
};
```

## Error Handling

Use `std::expected` (C++23) or custom Result type:

```cpp
template<typename T>
using Result = std::expected<T, Error>;

enum class ErrorCode {
    Ok,
    DimensionMismatch,
    VectorNotFound,
    IndexNotBuilt,
    InvalidParameter,
    IOError,
};
```

## Code Coverage

### Overview

Code coverage is a critical quality metric that measures the degree to which source code is executed during testing. For Lynx, maintaining high code coverage ensures reliability, aids in refactoring, and helps identify untested edge cases in the vector database implementation.

### Coverage Targets

| Component | Target Coverage | Priority |
|-----------|----------------|----------|
| Public APIs (IVectorDatabase) | 95-100% | Critical |
| Index Implementations (HNSW) | 90-95% | High |
| Distance Metrics | 95-100% | High |
| MPS Workers & Messages | 85-90% | High |
| Storage/Persistence | 85-90% | Medium |
| Utility Functions | 80-85% | Medium |

**Overall Project Target**: 85-90% line coverage, 75-80% branch coverage

### Coverage Types

1. **Line Coverage**: Percentage of code lines executed
   - Primary metric for overall code quality
   - Easiest to achieve and interpret

2. **Branch Coverage**: Percentage of decision branches taken
   - Critical for conditional logic (if/else, switch)
   - Ensures both true and false paths are tested
   - Harder to achieve than line coverage

3. **Function Coverage**: Percentage of functions called
   - Ensures all public APIs are exercised
   - Should be near 100% for public interfaces

4. **Region Coverage**: LLVM-specific, tracks code regions
   - Useful for identifying complex untested blocks

### Tools and Integration

#### GCC/G++ (gcov + lcov)

```bash
# Build with coverage flags
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage" \
      -B build-coverage

# Run tests
./build-coverage/bin/lynx_tests

# Generate coverage report
lcov --capture --directory build-coverage --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/external/*' '*/tests/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

#### Clang/LLVM (llvm-cov)

```bash
# Build with coverage flags
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
      -B build-coverage

# Run tests (generates profraw files)
LLVM_PROFILE_FILE="lynx_%p.profraw" ./build-coverage/bin/lynx_tests

# Merge profile data
llvm-profdata merge -sparse lynx_*.profraw -o lynx.profdata

# Generate HTML report
llvm-cov show ./build-coverage/bin/lynx_tests \
         -instr-profile=lynx.profdata \
         -format=html -output-dir=coverage_report \
         -ignore-filename-regex='(tests|external)'
```

### Build System Integration

Add to `CMakeLists.txt`:

```cmake
option(LYNX_ENABLE_COVERAGE "Enable code coverage reporting" OFF)

if(LYNX_ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    else()
        message(WARNING "Code coverage not supported for this compiler")
    endif()
endif()
```

Add to `setup.sh`:

```bash
coverage)
    check_dependencies
    log_info "Building with coverage instrumentation..."
    make coverage
    ;;
```

### Coverage Exclusions

Certain code should be excluded from coverage metrics:

1. **Test Code**: `tests/` directory
2. **External Dependencies**: `external/`, `/usr/include/*`
3. **Generated Code**: Auto-generated headers/sources
4. **Debug/Assertion Code**: Should be tested separately
5. **Unreachable Error Paths**: Some defensive checks

### Interpreting Coverage Data

#### Good Coverage Indicators

- All public API methods tested
- Both success and failure paths covered
- Edge cases (empty vectors, dimension=1, large N) tested
- Concurrent access patterns validated

#### Coverage Gaps to Investigate

- **Untested Error Handling**: Exception paths, validation failures
- **Missing Edge Cases**: Boundary conditions, empty inputs
- **Race Conditions**: Concurrent scenarios not covered
- **Platform-Specific Code**: SIMD fallbacks, OS differences

### Coverage in CI/CD

#### Pull Request Checks

1. **Coverage Diff**: Ensure new code is adequately tested
   - Fail if new code has <70% coverage
   - Report coverage change in PR comments

2. **Overall Coverage**: Track project-wide trends
   - Warn if total coverage drops >2%
   - Block merge if coverage drops >5%

3. **Critical Path Coverage**: Must-test components
   - IVectorDatabase interface: 100%
   - HNSW search/insert: 100%
   - Distance calculations: 100%

#### Nightly Builds

- Full coverage report generation
- Historical coverage tracking
- Coverage badge generation for README
- Email/Slack notifications on significant drops

### Best Practices

1. **Write Tests First**: TDD approach ensures coverage
2. **Test Public Contracts**: Focus on API behavior, not internals
3. **Cover Error Paths**: Test validation, exceptions, edge cases
4. **Avoid Coverage Gaming**: Don't write tests just to hit lines
5. **Review Uncovered Code**: Regularly audit low-coverage areas
6. **Use Coverage as Guide**: High coverage ≠ good tests, but helps identify gaps

### Implementation Roadmap

1. **Phase 1**: Setup tooling (gcov/lcov or llvm-cov)
   - Add CMake options
   - Integrate with setup.sh
   - Generate baseline report

2. **Phase 2**: Improve critical path coverage
   - IVectorDatabase: 100%
   - Distance metrics: 100%
   - HNSW core algorithms: 100%

3. **Phase 3**: CI/CD integration
   - GitHub Actions workflow
   - Coverage reporting
   - PR diff coverage

4. **Phase 4**: Maintenance
   - Regular coverage audits
   - Address gaps in low-coverage areas
   - Keep targets updated as code evolves

## Future Considerations

- **GPU Acceleration**: CUDA/OpenCL for distance calculations
- **Distributed Mode**: Sharding across multiple nodes
- **Sparse Vectors**: Support for sparse embeddings
- **Hybrid Search**: Combined vector + metadata filtering
- **Streaming Inserts**: High-throughput ingestion pipeline
- **IVF index implementation**: For faster indexing
- **Product Quantization (PQ)**: Compact storage
- **SIMD distance calculations**: Faster search and indexing
- **Extended candidate neighbors in heuristic selection**: Write a concept for this
- **Incremental index optimization/maintenance**: Write a concept for this
- **Benchmarks**: Testing against other vector databases
