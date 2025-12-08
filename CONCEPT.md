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
