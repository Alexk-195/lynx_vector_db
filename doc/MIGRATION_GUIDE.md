# Migration Guide: Unified VectorDatabase Architecture

> **Note**: As of 2025, the old database implementations (`VectorDatabase_Impl`, `VectorDatabase_MPS`, and `VectorDatabase_IVF`) have been removed from the codebase. This guide is retained for historical reference.

## Overview

Lynx has been unified around a single `VectorDatabase` class that works with all three index types (Flat, HNSW, IVF). This guide explains the migration from the old architecture to the new unified approach.

## What Changed?

### Old Architecture (Pre-2025)

Three separate database implementation classes:
- `VectorDatabase_Impl` - For Flat index
- `VectorDatabase_MPS` - For HNSW index (message-passing architecture)
- `VectorDatabase_IVF` - For IVF index

Each class had its own threading model and implementation strategy.

### New Architecture (Current)

**One unified database class**:
- `VectorDatabase` - Works with all index types (Flat, HNSW, IVF)
- Uses `std::shared_mutex` for thread safety
- Simpler, more maintainable code
- Consistent threading model across all index types

**Index abstraction**:
- `FlatIndex` - Brute-force exact search
- `HNSWIndex` - Hierarchical Navigable Small World graphs
- `IVFIndex` - Inverted File Index with k-means clustering

## Migration Steps

### Step 1: Update Your Code (Usually No Changes Needed!)

The good news: **Most code doesn't need to change** because the public `IVectorDatabase` interface remains the same.

#### Before (Old Code)
```cpp
#include "lynx/lynx.h"

lynx::Config config;
config.index_type = lynx::IndexType::HNSW;
config.dimension = 128;

// Factory method creates the appropriate implementation
auto db = lynx::IVectorDatabase::create(config);

// Use the database
db->insert(record);
auto results = db->search(query, k);
```

#### After (New Code)
```cpp
#include "lynx/lynx.h"

lynx::Config config;
config.index_type = lynx::IndexType::HNSW;  // Or Flat, or IVF
config.dimension = 128;

// Factory method now always creates unified VectorDatabase
auto db = lynx::IVectorDatabase::create(config);

// Use the database - API is identical!
db->insert(record);
auto results = db->search(query, k);
```

**Result**: Your code continues to work without modification!

### Step 2: Remove Direct Class References (If Any)

If you were directly instantiating implementation classes instead of using the factory, update your code:

#### Before (Discouraged Pattern)
```cpp
// DON'T: Direct instantiation of implementation classes
auto db_flat = std::make_shared<VectorDatabase_Impl>(config);
auto db_hnsw = std::make_shared<VectorDatabase_MPS>(config);
auto db_ivf = std::make_shared<VectorDatabase_IVF>(config);
```

#### After (Recommended Pattern)
```cpp
// DO: Use the factory method
config.index_type = lynx::IndexType::Flat;
auto db_flat = lynx::IVectorDatabase::create(config);

config.index_type = lynx::IndexType::HNSW;
auto db_hnsw = lynx::IVectorDatabase::create(config);

config.index_type = lynx::IndexType::IVF;
auto db_ivf = lynx::IVectorDatabase::create(config);
```

### Step 3: Update Configuration (Usually No Changes)

The `Config` structure is unchanged:

```cpp
struct Config {
    std::size_t dimension = 128;
    IndexType index_type = IndexType::HNSW;
    DistanceMetric distance_metric = DistanceMetric::L2;

    // HNSW parameters
    HNSWParams hnsw_params;

    // IVF parameters
    IVFParams ivf_params;

    std::string data_path = "./lynx_data";
};
```

**No migration needed** - existing config structures work as-is.

### Step 4: Update Threading Assumptions

#### Old Threading Models

- **VectorDatabase_Impl** (Flat): Simple `std::mutex`
- **VectorDatabase_MPS** (HNSW): Complex message-passing with multiple thread pools
- **VectorDatabase_IVF** (IVF): `std::shared_mutex`

#### New Unified Threading Model

**All index types** now use `std::shared_mutex`:
- ✅ **Concurrent reads**: Multiple threads can search simultaneously
- ✅ **Exclusive writes**: Inserts/removes are serialized
- ✅ **Simple and predictable**: Standard C++ synchronization
- ✅ **Sufficient for most use cases**: Handles < 50 concurrent queries efficiently

```cpp
// The unified VectorDatabase uses std::shared_mutex internally
// You don't need to worry about threading - it's handled automatically

// These operations are thread-safe:
std::thread t1([&]() { db->search(query1, k); });
std::thread t2([&]() { db->search(query2, k); });
std::thread t3([&]() { db->insert(record); });

t1.join();
t2.join();
t3.join();
```

## API Compatibility

### Unchanged APIs ✅

These interfaces remain **identical**:

```cpp
// Database operations
ErrorCode insert(const VectorRecord& record);
ErrorCode batch_insert(const std::vector<VectorRecord>& records);
ErrorCode remove(uint64_t id);
SearchResult search(const std::vector<float>& query, size_t k, const SearchParams& params = {});
std::optional<VectorRecord> get(uint64_t id);
bool contains(uint64_t id);
size_t size() const;
DatabaseStats stats() const;

// Persistence
ErrorCode flush();
ErrorCode save();
ErrorCode load();

// Maintenance
ErrorCode optimize_index();
ErrorCode clear();
```

### Behavior Changes

#### 1. Index Optimization

**Before** (VectorDatabase_MPS):
- Used WriteLog for non-blocking optimization
- Queries continued during optimization
- Complex clone-optimize-replay-swap pattern

**After** (Unified VectorDatabase):
- Blocks operations during optimization
- Simpler implementation
- Acceptable for most use cases (optimization is infrequent)

```cpp
// Optimization now blocks briefly
db->optimize_index();
// For HNSW: Rebuilds graph connections for better search performance
// For IVF: Re-runs k-means clustering and rebuilds inverted lists
// For Flat: No-op (nothing to optimize)
```

## Configuration Examples

### Example 1: Flat Index (Exact Search)

```cpp
lynx::Config config;
config.dimension = 128;
config.index_type = lynx::IndexType::Flat;
config.distance_metric = lynx::DistanceMetric::L2;

auto db = lynx::IVectorDatabase::create(config);
// Best for: Small datasets (<1K vectors), exact search required
// Speed: O(N), Recall: 100%
```

### Example 2: HNSW Index (High Performance)

```cpp
lynx::Config config;
config.dimension = 128;
config.index_type = lynx::IndexType::HNSW;
config.distance_metric = lynx::DistanceMetric::L2;

// HNSW parameters for performance tuning
config.hnsw_params.m = 16;                // Edges per node
config.hnsw_params.ef_construction = 200;  // Build quality
config.hnsw_params.ef_search = 50;        // Search quality

auto db = lynx::IVectorDatabase::create(config);
// Best for: High-performance ANN, low latency critical
// Speed: O(log N), Recall: 95-99%
```

### Example 3: IVF Index (Memory Efficient)

```cpp
lynx::Config config;
config.dimension = 128;
config.index_type = lynx::IndexType::IVF;
config.distance_metric = lynx::DistanceMetric::L2;

// IVF parameters (for ~10K vectors)
config.ivf_params.n_clusters = 100;  // sqrt(N) rule of thumb
config.ivf_params.n_probe = 10;      // Clusters to search

auto db = lynx::IVectorDatabase::create(config);
// Best for: Memory-constrained, fast construction needed
// Speed: Medium, Recall: 85-98%
```

## Performance Comparison

Based on integration testing (see `tickets/done/2057_test_results.md`):

| Metric | Old Architecture | New Unified Architecture |
|--------|-----------------|--------------------------|
| **Search Latency** | Baseline | Within ±5% |
| **Memory Usage** | Baseline | Similar or better |
| **Code Complexity** | High (3 classes) | Low (1 class) |
| **Threading Model** | Mixed | Unified (`std::shared_mutex`) |
| **Maintainability** | Difficult | Easy |
| **API Consistency** | Good | Excellent |

### When Performance Differs

**Unified VectorDatabase is better for**:
- Code maintainability
- API consistency
- Embedded systems (simpler threading)
- Low to medium concurrency (< 50 queries)

**Old VectorDatabase_MPS might have been slightly better for**:
- Extreme concurrency (100+ concurrent queries)
- Non-blocking index optimization required
- Advanced message-passing optimizations

**Note**: VectorDatabase_MPS code is preserved for future use if extreme performance is needed. See `doc/MPS_ARCHITECTURE.md` for details.

## Troubleshooting

### Issue: Performance Regression

**Symptom**: Queries are slower after migration

**Solution**: Check your index parameters:
```cpp
// For HNSW, increase ef_search for better recall
config.hnsw_params.ef_search = 100;  // Higher = slower but better

// For IVF, increase n_probe for better recall
config.ivf_params.n_probe = 20;  // Higher = slower but better
```

### Issue: High Memory Usage

**Symptom**: More memory used after migration

**Solution**: Consider switching to IVF index:
```cpp
config.index_type = lynx::IndexType::IVF;
config.ivf_params.n_clusters = static_cast<size_t>(std::sqrt(num_vectors));
config.ivf_params.n_probe = 10;
```

### Issue: Build Errors

**Symptom**: Compilation fails with missing classes

**Solution**: Ensure you're using the factory method:
```cpp
// Remove:
// #include "vector_database_impl.h"
// #include "vector_database_mps.h"
// #include "vector_database_ivf.h"

// Keep only:
#include "lynx/lynx.h"
auto db = lynx::IVectorDatabase::create(config);
```

## Advanced Topics

### When to Use VectorDatabase_MPS

Although the unified `VectorDatabase` is now the default, `VectorDatabase_MPS` remains available for advanced use cases:

```cpp
#include "lynx/lynx.h"

// Explicitly use MPS-based implementation for extreme performance
config.index_type = lynx::IndexType::HNSW;
auto db_mps = std::make_shared<VectorDatabase_MPS>(config);
```

**Use VectorDatabase_MPS when**:
- Serving 100+ concurrent queries
- Non-blocking index optimization required
- Profiling shows lock contention in VectorDatabase

See `doc/MPS_ARCHITECTURE.md` for detailed guidance.

### Index Selection Flowchart

```
Start: How many vectors?
│
├─ < 1K vectors
│  └─> Use Flat Index (exact search, simple)
│
├─ 1K - 1M vectors
│  │
│  ├─ Need exact search?
│  │  └─> Use Flat Index
│  │
│  ├─ Memory constrained?
│  │  └─> Use IVF Index
│  │     (n_clusters ≈ sqrt(N), n_probe ≈ 10)
│  │
│  └─ Need best performance?
│     └─> Use HNSW Index
│        (m=16, ef_construction=200, ef_search=50)
│
└─ > 1M vectors
   └─> Use IVF Index for memory efficiency
       OR HNSW Index for query speed
```

## Summary

### Key Takeaways

1. ✅ **Most code needs no changes** - Factory method handles everything
2. ✅ **Same API** - `IVectorDatabase` interface unchanged
3. ✅ **Simpler architecture** - One database class instead of three
4. ✅ **Unified threading** - `std::shared_mutex` for all index types
5. ✅ **Better maintainability** - Less code, clearer structure
6. ✅ **Preserved flexibility** - MPS still available for advanced cases

### Migration Checklist

- [ ] Update code to use `IVectorDatabase::create()` factory
- [ ] Remove direct instantiation of implementation classes (if any)
- [ ] Test with your existing configurations
- [ ] Verify performance meets requirements
- [ ] Update documentation/comments referencing old classes
- [ ] Consider index type based on your use case

### Getting Help

- **Documentation**: See `README.md` and `doc/MPS_ARCHITECTURE.md`
- **Examples**: Check `src/main.cpp` and `src/main_minimal.cpp`
- **Tests**: Review `tests/test_unified_database_integration.cpp`
- **Issues**: Report problems at GitHub issues

## Version History

- **2025-12-15**: Initial migration guide created
- **Architecture Change**: Unified VectorDatabase replaces three separate implementations
