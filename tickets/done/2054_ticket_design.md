# Unified VectorDatabase Design

**Date**: 2025-12-14
**Ticket**: #2054
**Author**: Claude (AI Assistant)
**Status**: Design Proposal

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Current State Analysis](#current-state-analysis)
3. [Design Goals](#design-goals)
4. [Architecture Overview](#architecture-overview)
5. [Class Structure](#class-structure)
6. [Threading Strategy](#threading-strategy)
7. [Common Operations](#common-operations)
8. [Vector Storage Strategy](#vector-storage-strategy)
9. [Statistics Tracking](#statistics-tracking)
10. [Persistence](#persistence)
11. [Index Delegation Pattern](#index-delegation-pattern)
12. [Error Handling Strategy](#error-handling-strategy)
13. [Migration Path](#migration-path)
14. [Implementation Phases](#implementation-phases)
15. [Testing Strategy](#testing-strategy)

---

## Executive Summary

This design document proposes a **unified VectorDatabase** class that consolidates three existing database implementations (`VectorDatabase_Impl`, `VectorDatabase_MPS`, `VectorDatabase_IVF`) into a single, maintainable implementation. The design uses the delegation pattern to abstract index-specific operations behind the `IVectorIndex` interface, while managing common concerns (vector storage, statistics, persistence, threading) at the database level.

**Key Benefits**:
- **Eliminates code duplication**: 70-80% of code is identical across implementations
- **Simplifies threading**: Replace complex MPS pools with `std::shared_mutex`
- **Improves maintainability**: Single codebase for all index types
- **Clean separation**: Database layer vs. Index layer responsibilities
- **Easier testing**: Unified test suite for all index types

---

## Current State Analysis

### Existing Implementations

#### 1. VectorDatabase_Impl (Flat Index)
**Location**: `src/lib/vector_database_flat.{h,cpp}`

**Structure**:
```cpp
class VectorDatabase_Impl : public IVectorDatabase {
private:
    Config config_;
    std::unordered_map<std::uint64_t, VectorRecord> vectors_;
    std::size_t total_inserts_;
    std::size_t total_queries_;
    double total_query_time_ms_;
};
```

**Characteristics**:
- Simple in-memory storage
- Brute-force search
- No threading (not thread-safe)
- Direct distance calculations
- ~350 lines of code

#### 2. VectorDatabase_MPS (HNSW Index)
**Location**: `src/lib/vector_database_hnsw.{h,cpp}`

**Structure**:
```cpp
class VectorDatabase_MPS : public IVectorDatabase {
private:
    Config config_;
    std::shared_ptr<HNSWIndex> index_;
    std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors_;

    // MPS threading
    std::vector<std::shared_ptr<mps::pool>> query_pools_;
    std::vector<std::shared_ptr<mps::pool>> index_pools_;
    std::shared_ptr<mps::pool> maintenance_pool_;

    // Distributors
    std::unique_ptr<PoolDistributor<SearchMessage>> search_distributor_;
    std::unique_ptr<PoolDistributor<InsertMessage>> insert_distributor_;
    // ... more distributors

    std::atomic<std::uint64_t> next_request_id_;
    std::shared_ptr<std::atomic<std::uint64_t>> total_inserts_;
    std::shared_ptr<std::atomic<std::uint64_t>> total_queries_;
    WriteLog write_log_;
    std::shared_mutex index_mutex_;
};
```

**Characteristics**:
- Complex MPS-based multi-threading
- N query pools + 2 index pools + 1 maintenance pool
- Message passing for all operations
- Non-blocking index optimization
- ~800+ lines of code (including workers/messages)

#### 3. VectorDatabase_IVF
**Location**: `src/lib/vector_database_ivf.{h,cpp}`

**Structure**:
```cpp
class VectorDatabase_IVF : public IVectorDatabase {
private:
    Config config_;
    std::shared_ptr<IVFIndex> index_;
    std::unordered_map<std::uint64_t, VectorRecord> vectors_;
    std::size_t total_inserts_;
    std::size_t total_queries_;
    double total_query_time_ms_;
    std::shared_mutex vectors_mutex_;
};
```

**Characteristics**:
- Simple threading with `std::shared_mutex`
- Delegates search to IVFIndex
- ~400 lines of code
- Most similar to proposed unified design

### Code Duplication Analysis

**Common Code** (present in all implementations):
- Vector storage: `std::unordered_map<uint64_t, VectorRecord>`
- Statistics tracking: `total_inserts_`, `total_queries_`, `total_query_time_ms_`
- Configuration management: `Config config_`
- Dimension validation
- Error handling
- `contains()`, `get()`, `size()`, `dimension()` implementations
- Persistence (save/load/flush) - similar patterns

**Estimated Duplication**: 70-80% of code is identical or very similar

---

## Design Goals

1. **Single Implementation**: One `VectorDatabase` class for all index types
2. **Delegation Pattern**: Database delegates index operations to `IVectorIndex*`
3. **Simple Threading**: Use `std::shared_mutex` (not MPS) for thread safety
4. **Clean Separation**: Database manages storage/stats/persistence, Index handles search algorithms
5. **Maintainability**: Reduce code duplication, easier to test and modify
6. **Backward Compatibility**: Maintain existing public API (`IVectorDatabase`)
7. **Performance**: No degradation compared to current implementations

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    Client Application                           │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                  IVectorDatabase (interface)                     │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    VectorDatabase (unified)                      │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │  Common Responsibilities:                                   │ │
│ │  - Vector storage (std::unordered_map)                      │ │
│ │  - Statistics tracking                                      │ │
│ │  - Thread safety (std::shared_mutex)                        │ │
│ │  - Persistence (save/load/flush)                            │ │
│ │  - Configuration management                                 │ │
│ │  - Dimension validation                                     │ │
│ └─────────────────────────────────────────────────────────────┘ │
│                              │                                   │
│                              │ (delegates to)                    │
│                              ▼                                   │
│ ┌─────────────────────────────────────────────────────────────┐ │
│ │              IVectorIndex* index_                           │ │
│ └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
      ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
      │  FlatIndex   │ │  HNSWIndex   │ │   IVFIndex   │
      │              │ │              │ │              │
      │ Brute-force  │ │ Graph-based  │ │ Clustering   │
      │   search     │ │    search    │ │   search     │
      └──────────────┘ └──────────────┘ └──────────────┘
```

**Responsibility Separation**:

| Layer | Responsibilities |
|-------|-----------------|
| **VectorDatabase** | Vector storage, statistics, threading, persistence, validation, metadata |
| **IVectorIndex** | Search algorithm, index structure, add/remove/search operations |

---

## Class Structure

### Header: `vector_database.h`

```cpp
/**
 * @file vector_database.h
 * @brief Unified vector database implementation for all index types
 *
 * This implementation uses the delegation pattern to support multiple
 * index types (Flat, HNSW, IVF) through a common IVectorIndex interface.
 * Thread safety is provided via std::shared_mutex for concurrent reads
 * and exclusive writes.
 */

#ifndef LYNX_VECTOR_DATABASE_H
#define LYNX_VECTOR_DATABASE_H

#include "../include/lynx/lynx.h"
#include "lynx_intern.h"
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <chrono>

namespace lynx {

/**
 * @brief Unified vector database implementation.
 *
 * This class provides a thread-safe implementation of IVectorDatabase
 * that works with any IVectorIndex implementation (Flat, HNSW, IVF).
 *
 * Features:
 * - In-memory vector storage with thread-safe access
 * - Delegates search operations to pluggable index implementations
 * - Statistics tracking (queries, inserts, memory usage)
 * - Persistence support (save/load)
 * - Concurrent read access, exclusive write access
 *
 * Thread Safety:
 * - Multiple concurrent readers (search, get, contains, stats)
 * - Exclusive writer access (insert, remove, batch_insert)
 * - Uses std::shared_mutex for reader-writer locking
 */
class VectorDatabase : public IVectorDatabase {
public:
    // -------------------------------------------------------------------------
    // Constructor and Destructor
    // -------------------------------------------------------------------------

    /**
     * @brief Construct a new vector database
     * @param config Database configuration
     *
     * Creates the appropriate index based on config.index_type:
     * - IndexType::Flat -> FlatIndex
     * - IndexType::HNSW -> HNSWIndex
     * - IndexType::IVF -> IVFIndex
     */
    explicit VectorDatabase(const Config& config);

    /**
     * @brief Destructor
     */
    ~VectorDatabase() override = default;

    // -------------------------------------------------------------------------
    // Single Vector Operations
    // -------------------------------------------------------------------------

    ErrorCode insert(const VectorRecord& record) override;
    ErrorCode remove(std::uint64_t id) override;
    bool contains(std::uint64_t id) const override;
    std::optional<VectorRecord> get(std::uint64_t id) const override;
    RecordRange all_records() const override;

    // -------------------------------------------------------------------------
    // Search Operations
    // -------------------------------------------------------------------------

    SearchResult search(std::span<const float> query, std::size_t k) const override;
    SearchResult search(std::span<const float> query, std::size_t k,
                       const SearchParams& params) const override;

    // -------------------------------------------------------------------------
    // Batch Operations
    // -------------------------------------------------------------------------

    ErrorCode batch_insert(std::span<const VectorRecord> records) override;

    // -------------------------------------------------------------------------
    // Database Properties
    // -------------------------------------------------------------------------

    std::size_t size() const override;
    std::size_t dimension() const override;
    DatabaseStats stats() const override;
    const Config& config() const override { return config_; }

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    ErrorCode flush() override;
    ErrorCode save() override;
    ErrorCode load() override;

private:
    // -------------------------------------------------------------------------
    // Helper Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Create index based on config.index_type
     * @return Shared pointer to IVectorIndex implementation
     */
    std::shared_ptr<IVectorIndex> create_index();

    /**
     * @brief Validate vector dimension
     * @param vector Vector to validate
     * @return ErrorCode::Ok if valid, ErrorCode::DimensionMismatch otherwise
     */
    ErrorCode validate_dimension(std::span<const float> vector) const;

    /**
     * @brief Get current time in milliseconds (for timing)
     * @return Current time as double (ms)
     */
    double get_time_ms() const;

    /**
     * @brief Check if IVF index should be rebuilt with new data
     * @param batch_size Size of batch to insert
     * @return true if rebuild would improve clustering quality
     */
    bool should_rebuild_ivf(std::size_t batch_size) const;

    /**
     * @brief Bulk build index from records (for empty index)
     * @param records Records to build index from
     * @return ErrorCode indicating success or failure
     */
    ErrorCode bulk_build(std::span<const VectorRecord> records);

    /**
     * @brief Rebuild IVF index with existing + new data
     * @param records New records to merge with existing data
     * @return ErrorCode indicating success or failure
     */
    ErrorCode rebuild_with_merge(std::span<const VectorRecord> records);

    /**
     * @brief Incremental insert (add vectors one by one)
     * @param records Records to insert incrementally
     * @return ErrorCode indicating success or failure
     */
    ErrorCode incremental_insert(std::span<const VectorRecord> records);

    // -------------------------------------------------------------------------
    // Member Variables
    // -------------------------------------------------------------------------

    // Configuration
    Config config_;                                           ///< Database configuration

    // Index (polymorphic - Flat, HNSW, or IVF)
    std::shared_ptr<IVectorIndex> index_;                    ///< Index implementation

    // Vector storage (shared with index for some operations)
    std::unordered_map<std::uint64_t, VectorRecord> vectors_; ///< Vector storage

    // Statistics
    std::atomic<std::size_t> total_inserts_{0};               ///< Total insert count
    std::atomic<std::size_t> total_queries_{0};               ///< Total query count
    std::atomic<double> total_query_time_ms_{0.0};            ///< Cumulative query time

    // Thread safety
    mutable std::shared_mutex mutex_;                         ///< Reader-writer lock
};

} // namespace lynx

#endif // LYNX_VECTOR_DATABASE_H
```

### Constructor Parameters

```cpp
explicit VectorDatabase(const Config& config);
```

The constructor:
1. Validates configuration parameters
2. Creates the appropriate index based on `config.index_type`
3. Initializes statistics counters
4. Sets up thread-safety primitives

---

## Threading Strategy

### Design Philosophy

**Simple Threading with std::shared_mutex**:
- Multiple concurrent readers (queries don't block each other)
- Exclusive writer access (writes block reads and other writes)
- No complex MPS infrastructure in core database
- Easier to reason about, maintain, and debug

### Lock Types

```cpp
mutable std::shared_mutex mutex_;
```

**Shared Lock (readers)**:
- `search()` - concurrent queries
- `get()` - concurrent lookups
- `contains()` - concurrent existence checks
- `stats()` - concurrent statistics access
- `all_records()` - iteration (holds lock for duration)

**Exclusive Lock (writers)**:
- `insert()` - modify vectors_ and index_
- `remove()` - modify vectors_ and index_
- `batch_insert()` - bulk modifications
- `save()` - serialize state
- `load()` - deserialize state

### Lock Acquisition Pattern

```cpp
// Read operation (shared lock)
SearchResult VectorDatabase::search(std::span<const float> query, size_t k) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    // Validate dimension
    // Delegate to index_->search()
    // Update statistics (atomic operations, no lock needed)
    // Return result
}

// Write operation (exclusive lock)
ErrorCode VectorDatabase::insert(const VectorRecord& record) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Validate dimension
    // Store vector in vectors_
    // Delegate to index_->add()
    // Update statistics
    // Return result
}
```

### Lock-Free Statistics

Statistics counters use `std::atomic` for lock-free updates:

```cpp
std::atomic<std::size_t> total_inserts_{0};
std::atomic<std::size_t> total_queries_{0};
std::atomic<double> total_query_time_ms_{0.0};
```

This allows statistics updates without holding locks, improving concurrency.

### Why Not MPS?

**MPS (Message Processing System)** was used in `VectorDatabase_MPS` for:
- Message passing between pools
- Non-blocking index optimization
- Complex load balancing

**Decision to Remove MPS**:
1. **Over-engineered**: Most use cases don't need message passing
2. **Complexity**: 800+ lines of code vs. ~400 with `std::shared_mutex`
3. **Maintenance**: MPS adds external dependency and complexity
4. **Performance**: `std::shared_mutex` is sufficient for typical workloads
5. **Simplicity**: Easier to understand and debug

**Non-blocking optimization** (from VectorDatabase_MPS) can be re-implemented later if needed, but it's not part of the core database design.

---

## Common Operations

### Insert Operation

```cpp
ErrorCode VectorDatabase::insert(const VectorRecord& record) {
    // 1. Validate dimension
    if (record.vector.size() != config_.dimension) {
        return ErrorCode::DimensionMismatch;
    }

    // 2. Acquire exclusive lock
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // 3. Check for duplicate ID
    if (vectors_.contains(record.id)) {
        // Option A: Return error
        // Option B: Allow overwrite (update)
        return ErrorCode::InvalidParameter; // or handle update
    }

    // 4. Store vector in vectors_
    vectors_[record.id] = record;

    // 5. Delegate to index
    ErrorCode result = index_->add(record.id, record.vector);
    if (result != ErrorCode::Ok) {
        // Rollback: remove from vectors_
        vectors_.erase(record.id);
        return result;
    }

    // 6. Update statistics
    total_inserts_.fetch_add(1, std::memory_order_relaxed);

    return ErrorCode::Ok;
}
```

### Search Operation

```cpp
SearchResult VectorDatabase::search(std::span<const float> query,
                                     std::size_t k,
                                     const SearchParams& params) const {
    // 1. Validate dimension
    if (query.size() != config_.dimension) {
        return SearchResult{};  // or return error via ErrorCode
    }

    // 2. Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // 3. Acquire shared lock
    std::shared_lock<std::shared_mutex> lock(mutex_);

    // 4. Delegate to index
    std::vector<SearchResultItem> items = index_->search(query, k, params);

    // 5. Release lock (RAII)
    lock.unlock();

    // 6. Calculate timing
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // 7. Update statistics (lock-free)
    total_queries_.fetch_add(1, std::memory_order_relaxed);

    // For total_query_time_ms_, use atomic operations
    double current = total_query_time_ms_.load(std::memory_order_relaxed);
    while (!total_query_time_ms_.compare_exchange_weak(current, current + elapsed_ms,
                                                         std::memory_order_relaxed)) {
        // Retry until successful
    }

    // 8. Build result
    SearchResult result;
    result.items = std::move(items);
    result.total_candidates = items.size();  // or get from index
    result.query_time_ms = elapsed_ms;

    return result;
}
```

### Remove Operation

```cpp
ErrorCode VectorDatabase::remove(std::uint64_t id) {
    // 1. Acquire exclusive lock
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // 2. Check if exists
    auto it = vectors_.find(id);
    if (it == vectors_.end()) {
        return ErrorCode::VectorNotFound;
    }

    // 3. Remove from index
    ErrorCode result = index_->remove(id);
    if (result != ErrorCode::Ok) {
        return result;
    }

    // 4. Remove from vectors_
    vectors_.erase(it);

    return ErrorCode::Ok;
}
```

### Batch Insert Operation

**Strategy**: Use a hybrid approach based on index state and type.

```cpp
ErrorCode VectorDatabase::batch_insert(std::span<const VectorRecord> records) {
    // 1. Validate all dimensions first
    for (const auto& record : records) {
        if (record.vector.size() != config_.dimension) {
            return ErrorCode::DimensionMismatch;
        }
    }

    // 2. Acquire exclusive lock
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // 3. HYBRID STRATEGY: Choose based on current state and index type

    // Strategy 1: Empty index → use bulk build (fastest)
    if (vectors_.empty()) {
        return bulk_build(records);
    }

    // Strategy 2: IVF with large batch → rebuild for better clustering
    if (config_.index_type == IndexType::IVF &&
        should_rebuild_ivf(records.size())) {
        return rebuild_with_merge(records);
    }

    // Strategy 3: Default → incremental insert (safest)
    return incremental_insert(records);
}

private:
    /**
     * @brief Check if IVF index should be rebuilt with new data
     * @param batch_size Size of batch to insert
     * @return true if rebuild would improve clustering quality
     */
    bool should_rebuild_ivf(size_t batch_size) const {
        // Rebuild if batch adds >50% more data
        // Rationale: k-means clustering with all data produces better centroids
        return batch_size > vectors_.size() * 0.5;
    }

    /**
     * @brief Bulk build index from records (for empty index)
     */
    ErrorCode bulk_build(std::span<const VectorRecord> records) {
        ErrorCode result = index_->build(records);
        if (result == ErrorCode::Ok) {
            for (const auto& record : records) {
                vectors_[record.id] = record;
            }
            total_inserts_.fetch_add(records.size(), std::memory_order_relaxed);
        }
        return result;
    }

    /**
     * @brief Rebuild IVF index with existing + new data
     *
     * For IVF, rebuilding with all data produces better k-means clusters
     * than incremental insertion. This is expensive (re-runs k-means) but
     * improves search quality.
     */
    ErrorCode rebuild_with_merge(std::span<const VectorRecord> records) {
        // Merge existing + new vectors
        std::vector<VectorRecord> all_records;
        all_records.reserve(vectors_.size() + records.size());

        // Add existing vectors
        for (const auto& [id, record] : vectors_) {
            all_records.push_back(record);
        }

        // Add new vectors
        all_records.insert(all_records.end(), records.begin(), records.end());

        // Rebuild index with all data
        ErrorCode result = index_->build(all_records);
        if (result == ErrorCode::Ok) {
            // Update vector storage
            for (const auto& record : records) {
                vectors_[record.id] = record;
            }
            total_inserts_.fetch_add(records.size(), std::memory_order_relaxed);
        }
        return result;
    }

    /**
     * @brief Incremental insert (add vectors one by one)
     *
     * Used for HNSW and Flat indices, or when IVF batch is small.
     * Maintains index structure incrementally.
     */
    ErrorCode incremental_insert(std::span<const VectorRecord> records) {
        for (const auto& record : records) {
            // Check for duplicate ID
            if (vectors_.contains(record.id)) {
                // Could allow updates or return error
                return ErrorCode::InvalidParameter;
            }

            // Store vector
            vectors_[record.id] = record;

            // Add to index
            ErrorCode result = index_->add(record.id, record.vector);
            if (result != ErrorCode::Ok) {
                // Rollback this insert
                vectors_.erase(record.id);
                // Return error (partial insert occurred for previous records)
                return result;
            }

            total_inserts_.fetch_add(1, std::memory_order_relaxed);
        }
        return ErrorCode::Ok;
    }
```

**Decision Matrix**:

| Condition | Strategy | Reason |
|-----------|----------|--------|
| `vectors_.empty()` | **Bulk Build** | Fastest - optimized construction |
| IVF + large batch (>50% existing) | **Rebuild with Merge** | Better k-means clustering quality |
| HNSW/Flat or small batch | **Incremental Insert** | Safe, maintains structure |

**Index-Specific Tradeoffs**:

| Index Type | Incremental | Bulk Build | Rebuild |
|------------|-------------|------------|---------|
| **Flat** | O(1) per insert | O(N) total | No difference |
| **HNSW** | O(log N) per insert | O(N log N) optimized | Usually not worth it |
| **IVF** | O(1) assignment | O(N·D·k·iters) | **Produces better clusters** |

**Why this matters for IVF**:
- K-means clustering quality depends on having all data
- Incremental adds assign to nearest centroid (may be suboptimal)
- Rebuilding recalculates centroids with all data → better partitioning → better search recall

---

## Vector Storage Strategy

### Storage Location

```cpp
std::unordered_map<std::uint64_t, VectorRecord> vectors_;
```

**Why VectorDatabase owns storage** (not the index):
1. **Separation of Concerns**: Database manages data, index manages search structure
2. **Metadata**: VectorRecord includes metadata that indices don't need
3. **Consistency**: Single source of truth for vector data
4. **Flexibility**: Indices can store just IDs, not full vectors (for some algorithms)

### Index Storage

**Different indices have different storage needs**:

| Index Type | Storage Strategy |
|------------|-----------------|
| **FlatIndex** | Stores full vectors (redundant with database, but needed for search) |
| **HNSWIndex** | Stores full vectors (needs them for distance calculations) |
| **IVFIndex** | Stores full vectors in inverted lists (redundant with database) |

**Design Decision**: Accept redundancy for simplicity. Future optimization could:
- Pass `vectors_` to index via shared_ptr (zero-copy)
- Have indices store only IDs and reference database vectors

### Memory Efficiency

**Current Design** (accept redundancy):
- Database: `N * (sizeof(VectorRecord))` ≈ `N * (D * 4 + 8 + metadata)`
- Index: `N * D * 4` (FlatIndex, IVFIndex)
- Total: `~2x` vector storage

**Future Optimization**:
- Share storage via `std::shared_ptr<std::unordered_map<...>>`
- Indices reference vectors by ID only
- Reduces memory by ~50%

For now, **prioritize simplicity over memory optimization**.

---

## Statistics Tracking

### Statistics Structure

```cpp
struct DatabaseStats {
    std::size_t vector_count;       // From vectors_.size()
    std::size_t dimension;          // From config_.dimension
    std::size_t memory_usage_bytes; // Approximate total memory
    std::size_t index_memory_bytes; // From index_->memory_usage()
    double avg_query_time_ms;       // total_query_time_ms_ / total_queries_
    std::size_t total_queries;      // From total_queries_
    std::size_t total_inserts;      // From total_inserts_
};
```

### Implementation

```cpp
DatabaseStats VectorDatabase::stats() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    DatabaseStats stats;
    stats.vector_count = vectors_.size();
    stats.dimension = config_.dimension;

    // Index memory
    stats.index_memory_bytes = index_->memory_usage();

    // Vector storage memory
    std::size_t vector_memory = vectors_.size() * (
        sizeof(VectorRecord) +
        config_.dimension * sizeof(float)
    );
    stats.memory_usage_bytes = vector_memory + stats.index_memory_bytes;

    // Query statistics
    stats.total_queries = total_queries_.load(std::memory_order_relaxed);
    stats.total_inserts = total_inserts_.load(std::memory_order_relaxed);

    double total_time = total_query_time_ms_.load(std::memory_order_relaxed);
    stats.avg_query_time_ms = (stats.total_queries > 0)
        ? (total_time / stats.total_queries)
        : 0.0;

    return stats;
}
```

### Thread-Safe Updates

```cpp
// Lock-free atomic updates
total_inserts_.fetch_add(1, std::memory_order_relaxed);
total_queries_.fetch_add(1, std::memory_order_relaxed);

// For double (query time), use compare-exchange
double current = total_query_time_ms_.load(std::memory_order_relaxed);
while (!total_query_time_ms_.compare_exchange_weak(
    current, current + elapsed_ms, std::memory_order_relaxed)) {
    // Retry until successful
}
```

---

## Persistence

### Save Operation

```cpp
ErrorCode VectorDatabase::save() {
    if (config_.data_path.empty()) {
        return ErrorCode::InvalidParameter; // No path configured
    }

    // Acquire shared lock (allows concurrent saves, blocks writes)
    std::shared_lock<std::shared_mutex> lock(mutex_);

    try {
        // 1. Save index
        std::string index_path = config_.data_path + "/index.bin";
        std::ofstream index_file(index_path, std::ios::binary);
        if (!index_file) {
            return ErrorCode::IOError;
        }

        ErrorCode result = index_->serialize(index_file);
        if (result != ErrorCode::Ok) {
            return result;
        }
        index_file.close();

        // 2. Save vectors (metadata)
        std::string vectors_path = config_.data_path + "/vectors.bin";
        std::ofstream vectors_file(vectors_path, std::ios::binary);
        if (!vectors_file) {
            return ErrorCode::IOError;
        }

        // Write header
        uint32_t magic = 0x4C594E58; // "LYNX"
        uint32_t version = 1;
        uint64_t count = vectors_.size();

        vectors_file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        vectors_file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        vectors_file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        // Write vectors with metadata
        for (const auto& [id, record] : vectors_) {
            vectors_file.write(reinterpret_cast<const char*>(&id), sizeof(id));

            // Write metadata length and data
            uint32_t meta_len = record.metadata.has_value()
                ? static_cast<uint32_t>(record.metadata->size()) : 0;
            vectors_file.write(reinterpret_cast<const char*>(&meta_len), sizeof(meta_len));
            if (meta_len > 0) {
                vectors_file.write(record.metadata->data(), meta_len);
            }
        }

        vectors_file.close();

        // 3. Save config
        std::string config_path = config_.data_path + "/config.bin";
        std::ofstream config_file(config_path, std::ios::binary);
        // ... serialize config ...

        return ErrorCode::Ok;

    } catch (const std::exception& e) {
        return ErrorCode::IOError;
    }
}
```

### Load Operation

```cpp
ErrorCode VectorDatabase::load() {
    if (config_.data_path.empty()) {
        return ErrorCode::InvalidParameter;
    }

    // Acquire exclusive lock
    std::unique_lock<std::shared_mutex> lock(mutex_);

    try {
        // 1. Load index
        std::string index_path = config_.data_path + "/index.bin";
        std::ifstream index_file(index_path, std::ios::binary);
        if (!index_file) {
            return ErrorCode::IOError;
        }

        ErrorCode result = index_->deserialize(index_file);
        if (result != ErrorCode::Ok) {
            return result;
        }
        index_file.close();

        // 2. Load vectors
        std::string vectors_path = config_.data_path + "/vectors.bin";
        std::ifstream vectors_file(vectors_path, std::ios::binary);
        if (!vectors_file) {
            return ErrorCode::IOError;
        }

        // Read header
        uint32_t magic, version;
        uint64_t count;
        vectors_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        vectors_file.read(reinterpret_cast<char*>(&version), sizeof(version));
        vectors_file.read(reinterpret_cast<char*>(&count), sizeof(count));

        if (magic != 0x4C594E58) {
            return ErrorCode::IOError; // Invalid file
        }

        // Read vectors
        vectors_.clear();
        for (uint64_t i = 0; i < count; ++i) {
            uint64_t id;
            vectors_file.read(reinterpret_cast<char*>(&id), sizeof(id));

            // Read metadata
            uint32_t meta_len;
            vectors_file.read(reinterpret_cast<char*>(&meta_len), sizeof(meta_len));
            std::optional<std::string> metadata;
            if (meta_len > 0) {
                std::string meta_str(meta_len, '\0');
                vectors_file.read(meta_str.data(), meta_len);
                metadata = meta_str;
            }

            // Reconstruct vector from index
            // (Index already has the vectors, we just need metadata)
            VectorRecord record{id, {}, metadata}; // vector is in index
            vectors_[id] = record;
        }

        vectors_file.close();

        return ErrorCode::Ok;

    } catch (const std::exception& e) {
        return ErrorCode::IOError;
    }
}
```

### Flush Operation

```cpp
ErrorCode VectorDatabase::flush() {
    // For in-memory databases, flush is a no-op
    // For persistent databases, same as save()
    return save();
}
```

---

## Index Delegation Pattern

### Index Creation

```cpp
std::shared_ptr<IVectorIndex> VectorDatabase::create_index() {
    switch (config_.index_type) {
        case IndexType::Flat:
            return std::make_shared<FlatIndex>(
                config_.dimension,
                config_.distance_metric
            );

        case IndexType::HNSW:
            return std::make_shared<HNSWIndex>(
                config_.dimension,
                config_.distance_metric,
                config_.hnsw_params
            );

        case IndexType::IVF:
            return std::make_shared<IVFIndex>(
                config_.dimension,
                config_.distance_metric,
                config_.ivf_params
            );

        default:
            throw std::invalid_argument("Unknown index type");
    }
}
```

### Delegation Examples

**Insert**:
```cpp
ErrorCode VectorDatabase::insert(const VectorRecord& record) {
    // ... validation, locking ...

    vectors_[record.id] = record;
    return index_->add(record.id, record.vector);  // DELEGATE
}
```

**Search**:
```cpp
SearchResult VectorDatabase::search(...) const {
    // ... validation, locking, timing ...

    std::vector<SearchResultItem> items = index_->search(query, k, params);  // DELEGATE

    // ... build SearchResult ...
}
```

**Remove**:
```cpp
ErrorCode VectorDatabase::remove(std::uint64_t id) {
    // ... locking, validation ...

    ErrorCode result = index_->remove(id);  // DELEGATE
    if (result == ErrorCode::Ok) {
        vectors_.erase(id);
    }
    return result;
}
```

### Index Interface Contract

**IVectorIndex guarantees**:
- `add()`: Insert vector into index
- `remove()`: Remove vector from index
- `search()`: Find k nearest neighbors
- `build()`: Bulk index construction
- `contains()`: Check existence
- `serialize()` / `deserialize()`: Persistence
- `size()`, `dimension()`, `memory_usage()`: Properties

**VectorDatabase responsibilities**:
- Store full VectorRecord (with metadata)
- Validate dimensions
- Track statistics
- Manage threading
- Handle persistence coordination

---

## Error Handling Strategy

### Error Propagation

```cpp
enum class ErrorCode {
    Ok = 0,
    DimensionMismatch,
    VectorNotFound,
    IndexNotBuilt,
    InvalidParameter,
    InvalidState,
    OutOfMemory,
    IOError,
    NotImplemented,
    Busy,
};
```

### Error Handling Patterns

**Return ErrorCode**:
```cpp
ErrorCode VectorDatabase::insert(const VectorRecord& record) {
    if (record.vector.size() != config_.dimension) {
        return ErrorCode::DimensionMismatch;
    }
    // ...
}
```

**Validate Before Index Delegation**:
```cpp
ErrorCode VectorDatabase::insert(const VectorRecord& record) {
    // Database validates
    if (record.vector.size() != config_.dimension) {
        return ErrorCode::DimensionMismatch;
    }

    // Delegate to index
    ErrorCode result = index_->add(record.id, record.vector);
    if (result != ErrorCode::Ok) {
        // Rollback if needed
        vectors_.erase(record.id);
    }

    return result;
}
```

**Atomic Operations** (rollback on failure):
```cpp
ErrorCode VectorDatabase::insert(const VectorRecord& record) {
    // 1. Store in vectors_
    vectors_[record.id] = record;

    // 2. Add to index
    ErrorCode result = index_->add(record.id, record.vector);

    // 3. Rollback on failure
    if (result != ErrorCode::Ok) {
        vectors_.erase(record.id);
        return result;
    }

    return ErrorCode::Ok;
}
```

### Exception Safety

- **No exceptions in normal operations**: Use ErrorCode return values
- **Exceptions for programmer errors**: Invalid config, null pointers
- **RAII for lock management**: std::unique_lock, std::shared_lock
- **Strong exception guarantee**: Rollback on failure

---

## Migration Path

### Phase 1: Implementation (Week 1)

**Tasks**:
1. Create `src/lib/vector_database.h`
2. Create `src/lib/vector_database.cpp`
3. Implement constructor + `create_index()`
4. Implement insert/remove/search operations
5. Implement statistics tracking
6. Implement persistence (save/load)

**Testing**:
- Unit tests for each operation
- Test with all three index types (Flat, HNSW, IVF)
- Thread-safety tests (concurrent reads/writes)

### Phase 2: Factory Refactoring (Week 1)

**Modify**: `src/lib/lynx.cpp`

```cpp
std::shared_ptr<IVectorDatabase> IVectorDatabase::create(const Config& config) {
    // OLD CODE (switch on index type to create different database classes)
    /*
    switch (config.index_type) {
        case IndexType::Flat:
            return std::make_shared<VectorDatabase_Impl>(config);
        case IndexType::HNSW:
            return std::make_shared<VectorDatabase_MPS>(config);
        case IndexType::IVF:
            return std::make_shared<VectorDatabase_IVF>(config);
    }
    */

    // NEW CODE (single database class for all index types)
    return std::make_shared<VectorDatabase>(config);
}
```

### Phase 3: Deprecation (Week 2)

**Mark old implementations as deprecated**:
```cpp
// vector_database_flat.h
[[deprecated("Use VectorDatabase instead")]]
class VectorDatabase_Impl : public IVectorDatabase { ... };

// vector_database_hnsw.h
[[deprecated("Use VectorDatabase instead")]]
class VectorDatabase_MPS : public IVectorDatabase { ... };

// vector_database_ivf.h
[[deprecated("Use VectorDatabase instead")]]
class VectorDatabase_IVF : public IVectorDatabase { ... };
```

### Phase 4: Testing & Validation (Week 2)

**Validation**:
1. Run all existing tests with new implementation
2. Benchmark performance (ensure no regression)
3. Test migration from old save files
4. Stress test thread safety

**Success Criteria**:
- All tests pass
- Performance within 5% of old implementations
- No memory leaks
- Thread-safety verified

### Phase 5: Removal (Week 3)

**Remove old implementations**:
1. Delete `vector_database_flat.{h,cpp}`
2. Delete `vector_database_hnsw.{h,cpp}`
3. Delete `vector_database_ivf.{h,cpp}`
4. Delete `mps_workers.{h,cpp}` (if only used by database)
5. Delete `mps_messages.h` (if only used by database)
6. Update documentation

### Rollback Plan

If issues are discovered:
1. Revert factory changes in `lynx.cpp`
2. Keep new `VectorDatabase` as alternative (not default)
3. Fix issues before retry
4. Old implementations remain available during transition

---

## Implementation Phases

### Phase 1: Core Implementation
**Duration**: 3-4 days
**Files**:
- `src/lib/vector_database.h`
- `src/lib/vector_database.cpp`

**Tasks**:
- [ ] Create header with class definition
- [ ] Implement constructor + index factory
- [ ] Implement insert/remove/contains/get
- [ ] Implement search (delegate to index)
- [ ] Implement batch_insert
- [ ] Implement statistics tracking
- [ ] Implement all_records() iterator

### Phase 2: Persistence
**Duration**: 2 days
**Tasks**:
- [ ] Implement save() - serialize vectors + index
- [ ] Implement load() - deserialize vectors + index
- [ ] Implement flush()
- [ ] Test persistence with all index types

### Phase 3: Testing
**Duration**: 3 days
**Files**:
- `tests/test_vector_database.cpp`

**Tasks**:
- [ ] Unit tests for all operations
- [ ] Thread-safety tests (concurrent reads/writes)
- [ ] Test with Flat, HNSW, IVF indices
- [ ] Persistence tests
- [ ] Memory leak tests (valgrind)
- [ ] Performance benchmarks

### Phase 4: Integration
**Duration**: 2 days
**Tasks**:
- [ ] Update factory in `lynx.cpp`
- [ ] Run all existing tests
- [ ] Update documentation
- [ ] Deprecate old implementations

### Phase 5: Cleanup
**Duration**: 1 day
**Tasks**:
- [ ] Remove old database implementations
- [ ] Remove MPS workers/messages (if unused)
- [ ] Update CONCEPT.md
- [ ] Update tickets

---

## Testing Strategy

### Unit Tests

```cpp
// Test basic operations with FlatIndex
TEST(VectorDatabaseTest, InsertSearchFlat) {
    Config config;
    config.dimension = 4;
    config.index_type = IndexType::Flat;

    auto db = std::make_shared<VectorDatabase>(config);

    // Insert
    VectorRecord record{1, {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt};
    EXPECT_EQ(db->insert(record), ErrorCode::Ok);

    // Search
    std::vector<float> query = {1.0f, 2.0f, 3.0f, 4.0f};
    auto result = db->search(query, 1);
    EXPECT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].id, 1);
}

// Test with HNSWIndex
TEST(VectorDatabaseTest, InsertSearchHNSW) {
    Config config;
    config.dimension = 4;
    config.index_type = IndexType::HNSW;

    auto db = std::make_shared<VectorDatabase>(config);
    // ... similar tests ...
}

// Test thread safety
TEST(VectorDatabaseTest, ConcurrentReads) {
    Config config;
    config.dimension = 128;
    config.index_type = IndexType::HNSW;

    auto db = std::make_shared<VectorDatabase>(config);

    // Insert 1000 vectors
    for (int i = 0; i < 1000; ++i) {
        VectorRecord record{static_cast<uint64_t>(i),
                           std::vector<float>(128, i * 0.1f),
                           std::nullopt};
        db->insert(record);
    }

    // Launch 10 concurrent search threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&db]() {
            std::vector<float> query(128, 0.5f);
            for (int j = 0; j < 100; ++j) {
                auto result = db->search(query, 10);
                EXPECT_GE(result.items.size(), 1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
}

// Test persistence
TEST(VectorDatabaseTest, SaveLoad) {
    Config config;
    config.dimension = 4;
    config.index_type = IndexType::IVF;
    config.data_path = "/tmp/lynx_test";

    // Create and populate
    auto db1 = std::make_shared<VectorDatabase>(config);
    db1->insert({1, {1, 2, 3, 4}, std::nullopt});
    db1->insert({2, {5, 6, 7, 8}, std::nullopt});
    EXPECT_EQ(db1->save(), ErrorCode::Ok);

    // Load in new instance
    auto db2 = std::make_shared<VectorDatabase>(config);
    EXPECT_EQ(db2->load(), ErrorCode::Ok);

    // Verify
    EXPECT_EQ(db2->size(), 2);
    EXPECT_TRUE(db2->contains(1));
    EXPECT_TRUE(db2->contains(2));
}
```

### Performance Benchmarks

```cpp
// Benchmark insert performance
BENCHMARK(VectorDatabaseBench, InsertHNSW) {
    Config config;
    config.dimension = 128;
    config.index_type = IndexType::HNSW;

    auto db = std::make_shared<VectorDatabase>(config);

    for (int i = 0; i < 10000; ++i) {
        VectorRecord record{static_cast<uint64_t>(i),
                           std::vector<float>(128, i * 0.01f),
                           std::nullopt};
        db->insert(record);
    }
}

// Benchmark search performance
BENCHMARK(VectorDatabaseBench, SearchHNSW) {
    // Setup: insert 10000 vectors
    // Run: 1000 searches
    // Measure: queries per second
}
```

---

## Conclusion

This design provides a **clean, maintainable, and efficient** solution for unifying the three existing VectorDatabase implementations. Key benefits include:

1. **70-80% code reduction** by eliminating duplication
2. **Simpler threading** with `std::shared_mutex` instead of complex MPS
3. **Clear separation of concerns** between database and index layers
4. **Backward compatible** via factory pattern
5. **Easier to test and maintain**
6. **Extensible** for future index types

The implementation can proceed incrementally with a clear migration path and rollback plan if issues arise.

**Next Steps**:
1. Review and approve this design
2. Create ticket #2055 for implementation
3. Begin Phase 1: Core Implementation
