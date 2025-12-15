# MPS Architecture Documentation

## Table of Contents

1. [Overview](#overview)
2. [MPS Core Components](#mps-core-components)
3. [VectorDatabase_MPS Architecture](#vectordatabase_mps-architecture)
4. [Message Types and Workers](#message-types-and-workers)
5. [Non-Blocking Maintenance](#non-blocking-maintenance)
6. [Performance Characteristics](#performance-characteristics)
7. [When to Use MPS](#when-to-use-mps)
8. [Implementation Files](#implementation-files)

## Overview

The MPS (Message Processing System) infrastructure in Lynx provides a high-performance, thread-safe implementation of the vector database using message-passing architecture. While the default `VectorDatabase` uses simple `std::shared_mutex` for thread safety, `VectorDatabase_MPS` offers advanced capabilities for extreme performance requirements.

**Key Principle**: Message passing instead of shared state with locks.

## MPS Core Components

### 1. MPS Pool (`mps::pool`)

A pool is a **thread + message queue** combination. Each pool:
- Runs on its own dedicated thread
- Has an internal message queue (FIFO)
- Processes messages sequentially using a worker
- Can be started, stopped, and joined

**Example**:
```cpp
// Create a pool with a worker
auto pool = std::make_shared<mps::pool>(std::make_shared<QueryWorker>(...));
pool->start();  // Starts the thread

// Send messages to the pool
pool->push_back(message);
```

### 2. MPS Worker (`mps::worker`)

A worker defines **how to process messages**. Workers:
- Inherit from `mps::worker` base class
- Implement `process(std::shared_ptr<const mps::message> msg)` method
- Use dynamic casting to handle different message types
- Run inside a pool's thread

**Example**:
```cpp
class QueryWorker : public mps::worker {
public:
    void process(std::shared_ptr<const mps::message> msg) override {
        if (auto search_msg = std::dynamic_pointer_cast<const SearchMessage>(msg)) {
            // Handle search operation
            auto results = index_->search(search_msg->query, search_msg->k);
            search_msg->set_value(std::move(results));
        }
        // ... handle other message types
    }
};
```

### 3. MPS Message (`mps::message`)

Messages are **units of work** sent to pools. Each message:
- Inherits from `mps::message` base class
- Contains data for the operation
- Has a promise/future pair for async results
- Is processed by workers

**Example**:
```cpp
struct SearchMessage : DatabaseMessage<SearchResult> {
    std::vector<float> query;
    std::size_t k;
    SearchParams params;

    // Inherited from DatabaseMessage:
    // - std::promise<SearchResult> promise
    // - std::future<SearchResult> get_future()
    // - void set_value(SearchResult result)
};
```

### 4. Pool Distributor (`PoolDistributor<T>`)

A distributor provides **load balancing** across multiple pools using round-robin strategy:

```cpp
template<typename MessageType>
class PoolDistributor {
public:
    explicit PoolDistributor(std::vector<std::shared_ptr<mps::pool>> pools);

    // Send message to next pool (round-robin)
    void send(std::shared_ptr<MessageType> msg);

    size_t pool_count() const;
};
```

**Benefits**:
- Distributes load evenly across pools/threads
- Simple round-robin (counter % pool_count)
- No thread contention on the counter (uses atomic)

## VectorDatabase_MPS Architecture

`VectorDatabase_MPS` uses a **multi-pool architecture** for different operation types:

```
┌─────────────────────────────────────────────────────────────┐
│                    VectorDatabase_MPS                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Query Pools (N threads, default: CPU cores)          │  │
│  │  - Parallel read operations                           │  │
│  │  - QueryWorker instances                              │  │
│  │  - Round-robin distribution                           │  │
│  │                                                        │  │
│  │  Messages: Search, Get, Contains, Stats               │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↓↑                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           Shared HNSW Index + Vector Storage          │  │
│  │           (protected by shared_mutex)                 │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↓↑                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Index Pools (2 threads)                              │  │
│  │  - Concurrent write operations                        │  │
│  │  - IndexWorker instances                              │  │
│  │  - HNSW provides write serialization                  │  │
│  │                                                        │  │
│  │  Messages: Insert, BatchInsert, Remove                │  │
│  └───────────────────────────────────────────────────────┘  │
│                           ↓↑                                 │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Maintenance Pool (1 thread)                          │  │
│  │  - Background tasks                                    │  │
│  │  - MaintenanceWorker instance                         │  │
│  │                                                        │  │
│  │  Messages: Flush, Save, Load                          │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Pool Configuration

**Query Pools** (N threads):
- Default: `std::thread::hardware_concurrency()` (number of CPU cores)
- Handle all read operations (search, get, contains, stats)
- Workers access index with shared locks (concurrent reads)
- Round-robin load balancing across pools

**Index Pools** (2 threads):
- Handle all write operations (insert, batch_insert, remove)
- Workers access index with exclusive locks (serialized writes)
- HNSW's internal `shared_mutex` provides synchronization

**Maintenance Pool** (1 thread):
- Handles persistence operations (flush, save, load)
- Single thread avoids concurrency issues
- Non-blocking index optimization (see below)

### Thread Safety Model

```cpp
// Shared data structures
std::shared_ptr<HNSWIndex> index_;                      // Protected by HNSWIndex::shared_mutex_
std::shared_ptr<std::unordered_map<...>> vectors_;      // Protected by message serialization
std::shared_ptr<std::atomic<uint64_t>> total_queries_;  // Lock-free atomic
std::shared_ptr<std::atomic<uint64_t>> total_inserts_;  // Lock-free atomic
```

**Read Path** (QueryWorker):
1. Message arrives at query pool
2. Worker acquires shared lock on index
3. Multiple queries can run concurrently
4. Result sent back via promise

**Write Path** (IndexWorker):
1. Message arrives at index pool
2. Worker acquires exclusive lock on index
3. Operation performed (add/remove)
4. Vectors map updated
5. Result sent back via promise

## Message Types and Workers

### Query Messages (READ operations)

Processed by `QueryWorker` in query pools:

| Message Type | Operation | Returns | Concurrency |
|--------------|-----------|---------|-------------|
| `SearchMessage` | Vector similarity search | `SearchResult` | Parallel (shared lock) |
| `GetMessage` | Get vector by ID | `std::optional<VectorRecord>` | Parallel (shared lock) |
| `ContainsMessage` | Check if ID exists | `bool` | Parallel (shared lock) |
| `StatsMessage` | Get database statistics | `DatabaseStats` | Parallel (lock-free atomics) |

**Implementation**:
```cpp
class QueryWorker : public mps::worker {
    void process_search(std::shared_ptr<const SearchMessage> msg) {
        total_queries_->fetch_add(1, std::memory_order_relaxed);
        auto start = std::chrono::high_resolution_clock::now();

        // Concurrent read access (shared lock inside index->search)
        auto items = index_->search(msg->query, msg->k, msg->params);

        auto end = std::chrono::high_resolution_clock::now();
        double query_time_ms = /* calculate duration */;

        SearchResult result{std::move(items), index_->size(), query_time_ms};
        msg->set_value(std::move(result));
    }
};
```

### Index Messages (WRITE operations)

Processed by `IndexWorker` in index pools:

| Message Type | Operation | Returns | Concurrency |
|--------------|-----------|---------|-------------|
| `InsertMessage` | Insert single vector | `ErrorCode` | Serialized (exclusive lock) |
| `BatchInsertMessage` | Insert multiple vectors | `ErrorCode` | Serialized (exclusive lock) |
| `RemoveMessage` | Remove vector by ID | `ErrorCode` | Serialized (exclusive lock) |

**Implementation**:
```cpp
class IndexWorker : public mps::worker {
    void process_insert(std::shared_ptr<const InsertMessage> msg) {
        const auto& record = msg->record;

        // Exclusive write access (exclusive lock inside index->add)
        ErrorCode result = index_->add(record.id, record.vector);

        // Handle duplicate ID
        if (result == ErrorCode::InvalidState) {
            index_->remove(record.id);
            result = index_->add(record.id, record.vector);
        }

        if (result == ErrorCode::Ok) {
            (*vectors_)[record.id] = record;
            total_inserts_->fetch_add(1, std::memory_order_relaxed);
        }

        msg->set_value(result);
    }
};
```

### Maintenance Messages (BACKGROUND operations)

Processed by `MaintenanceWorker` in maintenance pool:

| Message Type | Operation | Returns | Description |
|--------------|-----------|---------|-------------|
| `FlushMessage` | Flush to disk | `ErrorCode` | Currently a no-op (for future WAL) |
| `SaveMessage` | Save index + metadata | `ErrorCode` | Serialize to `data_path/` |
| `LoadMessage` | Load index + metadata | `ErrorCode` | Deserialize from `data_path/` |

**Note**: Index optimization (`optimize_index()`) is handled at the database level, not via messages, because it requires coordination with the write log.

## Non-Blocking Maintenance

### Problem

Traditional index maintenance (optimization, compaction) blocks all operations:
```cpp
// Traditional approach (blocks everything)
lock.lock();
optimize_index();  // Takes seconds or minutes
lock.unlock();
```

**Issues**:
- Query latency spikes during maintenance
- Poor user experience
- Wasted CPU cycles (queries blocked while CPU is available)

### Solution: Clone-Optimize-Replay-Swap

`VectorDatabase_MPS` uses a **WriteLog** to enable non-blocking maintenance:

```cpp
ErrorCode VectorDatabase_MPS::optimize_index() {
    // 1. Enable write logging
    write_log_.enabled.store(true);
    write_log_.clear();

    // 2. Clone the current index
    auto optimized_index = clone_index(index_);

    // 3. Optimize the clone (queries continue on active index!)
    optimized_index->optimize();

    // 4. Replay logged writes to optimized clone
    write_log_.replay_to(optimized_index.get());

    // 5. Atomically swap active index
    {
        std::unique_lock lock(index_mutex_);
        index_ = optimized_index;
    }

    // 6. Disable write logging
    write_log_.enabled.store(false);
    write_log_.clear();

    return ErrorCode::Ok;
}
```

**Flow Diagram**:
```
Time →

T0: Start maintenance
    ├─ Enable WriteLog
    ├─ Clone index (Index_v1 → Index_v1_clone)
    │
    ├─ Queries continue on Index_v1 (active)
    │
T1: Optimize clone in background
    ├─ Index_v1_clone.optimize()  ← CPU-intensive work
    ├─ Meanwhile: writes logged to WriteLog
    │
T2: Replay and swap
    ├─ Replay WriteLog to Index_v1_clone
    ├─ Atomic swap: Index_v1_clone becomes active
    ├─ Disable WriteLog
    │
T3: Done
    └─ Queries now use optimized index
```

### WriteLog Design

```cpp
struct WriteLog {
    enum class Operation { Insert, Remove };

    struct Entry {
        Operation op;
        std::uint64_t id;
        std::vector<float> vector;
        std::chrono::steady_clock::time_point timestamp;
    };

    static constexpr std::size_t kMaxEntries = 100'000;
    static constexpr std::size_t kWarnThreshold = 50'000;

    std::vector<Entry> entries;      // Ordered chronologically
    mutable std::mutex mutex;         // Thread-safe access
    std::atomic<bool> enabled{false}; // Toggle logging

    bool log_insert(std::uint64_t id, std::span<const float> vector);
    bool log_remove(std::uint64_t id);
    void replay_to(HNSWIndex* target);
    void clear();
};
```

**Key Properties**:
- **Ordered**: `std::vector` preserves operation order (critical for correctness)
- **Bounded**: Max 100K entries (prevents unbounded memory growth)
- **Thread-safe**: Own mutex for concurrent write logging
- **Abortable**: Returns false if log overflows (abort maintenance)

**Benefits**:
- ✅ Zero query blocking during maintenance
- ✅ Consistent low latency
- ✅ Correct handling of concurrent writes via ordered replay
- ✅ Graceful degradation if too many writes (abort optimization)

## Performance Characteristics

### Comparison: VectorDatabase vs VectorDatabase_MPS

Based on integration testing (see `tickets/done/2057_test_results.md`):

| Metric | VectorDatabase | VectorDatabase_MPS | Notes |
|--------|----------------|-------------------|-------|
| **Search Latency** | Baseline | Within ±20% | MPS overhead minimal |
| **Thread Safety** | `std::shared_mutex` | Message passing | Different synchronization models |
| **Memory Usage** | 1x | ~1x | Similar memory footprint |
| **Complexity** | Low | High | MPS requires understanding message passing |
| **Blocking Maintenance** | Yes | **No** | MPS supports non-blocking optimization |
| **Query Throughput** | High | **Very High** | MPS better for extreme concurrency |

### When VectorDatabase_MPS is Faster

1. **Very High Concurrency** (100+ concurrent queries)
   - Message passing scales better than locks under contention
   - Each pool has dedicated thread, no context switching overhead

2. **Non-Blocking Maintenance Required**
   - WriteLog pattern eliminates query blocking
   - Critical for applications with strict latency SLAs

3. **Advanced Optimization**
   - MPS provides hooks for custom optimization strategies
   - Can implement custom message types and workers

### When VectorDatabase is Sufficient

1. **Low to Medium Concurrency** (< 50 concurrent queries)
   - `std::shared_mutex` has less overhead for typical loads
   - Simpler codebase, easier to debug

2. **Small Datasets** (< 100K vectors)
   - Maintenance is fast enough that blocking is acceptable
   - No need for complex non-blocking patterns

3. **Embedded Systems**
   - Lower memory overhead (no message queues)
   - Simpler threading model

## When to Use MPS

### Use VectorDatabase_MPS When:

✅ **Very High Concurrency Requirements**
- Serving 100+ concurrent queries
- Query throughput is critical
- Multi-core system (8+ cores) available

✅ **Non-Blocking Operations Required**
- Strict latency SLAs (e.g., P99 < 10ms)
- Cannot tolerate query blocking during maintenance
- Index optimization must run continuously

✅ **Advanced Optimization Scenarios**
- Custom message types for specific workflows
- Need hooks into processing pipeline
- Implementing advanced load balancing strategies

✅ **Production Systems with High Load**
- Vector database is primary bottleneck
- Query performance profiling shows lock contention
- Budget for increased complexity

### Use VectorDatabase When:

✅ **Default Choice** (simpler and sufficient for most use cases)
- Low to medium concurrency (< 50 queries)
- Small to medium datasets (< 1M vectors)
- Maintenance downtime is acceptable

✅ **Embedded Systems**
- Limited CPU cores (< 4 cores)
- Memory constrained
- Simpler architecture preferred

✅ **Development and Prototyping**
- Faster iteration cycles
- Easier debugging with standard locks
- Can migrate to MPS later if needed

### Migration Path

Start with `VectorDatabase`, migrate to `VectorDatabase_MPS` if:
1. Query latency P99 > 10ms under load
2. CPU utilization shows lock contention
3. Maintenance causes noticeable query blocking

**Example**:
```cpp
// Start simple
Config config;
config.index_type = IndexType::HNSW;
auto db = std::make_shared<VectorDatabase>(config);

// Migrate to MPS if needed (same API!)
auto db_mps = std::make_shared<VectorDatabase_MPS>(config);
// API is identical, just drop-in replacement
```

## Implementation Files

### Core MPS Files

| File | Purpose |
|------|---------|
| `src/lib/vector_database_hnsw.h` | VectorDatabase_MPS implementation + PoolDistributor |
| `src/lib/vector_database_hnsw.cpp` | VectorDatabase_MPS implementation |
| `src/lib/mps_messages.h` | All message type definitions |
| `src/lib/mps_workers.h` | QueryWorker, IndexWorker, MaintenanceWorker |
| `src/lib/write_log.h` | WriteLog for non-blocking maintenance |

### External MPS Library

| File | Purpose |
|------|---------|
| `external/mps/src/mps.h` | Core MPS: pool, worker, message base classes |
| `external/mps/src/mps_message.h` | Message base class |
| `external/mps/src/mps_synchronized.h` | Thread-safe wrapper (not used in Lynx) |

### Tests

| File | Purpose |
|------|---------|
| `tests/test_mps.cpp` | MPS-specific tests |
| `tests/test_threading.cpp` | Thread safety tests (both VectorDatabase and VectorDatabase_MPS) |
| `tests/test_unified_database_integration.cpp` | Performance comparison tests |

## Further Reading

- [MPS Library Documentation](https://github.com/Alexk-195/mps) - Core MPS concepts
- [HNSW Paper](https://arxiv.org/abs/1603.09320) - Algorithm background
- [Integration Test Results](../tickets/done/2057_test_results.md) - Performance data
- [Database Architecture Analysis](../tickets/done/2051_analysis.md) - Design decisions

## Summary

**MPS Infrastructure** provides:
- High-performance message-passing architecture
- Non-blocking index maintenance via WriteLog
- Excellent scalability for high-concurrency scenarios
- Production-ready for demanding workloads

**Default VectorDatabase** provides:
- Simpler architecture with standard locks
- Sufficient performance for most use cases
- Lower complexity and easier debugging
- Better choice for embedded systems

**Choose based on your requirements**: Start simple with `VectorDatabase`, migrate to `VectorDatabase_MPS` only when profiling shows it's necessary.
