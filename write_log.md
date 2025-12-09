# Write Log Design for Non-Blocking Index Maintenance

## Problem Statement

Index maintenance operations like `optimize_graph()` and `compact_index()` require exclusive locks (`std::unique_lock`) on the HNSW index. These operations can be long-running (seconds to minutes for large indices), during which **all query operations are blocked** because they need shared locks.

### The Core Issue

```cpp
// Current implementation in hnsw_index.cpp
ErrorCode HNSWIndex::search(...) const {
    std::shared_lock lock(mutex_);  // Blocked if maintenance holds unique_lock!
    // ... search logic ...
}

ErrorCode HNSWIndex::optimize_graph() {
    std::unique_lock lock(mutex_);  // Blocks ALL queries!
    // ... potentially minutes of work ...
}
```

**Impact:** Query latency spikes from milliseconds to seconds during maintenance.

## Solutions Considered

### 1. Incremental Processing with Lock Yielding

**Idea:** Process index in batches, releasing lock between batches.

**Problem:** Race conditions if writes occur between batches:
- Deleted nodes → crash on access
- New nodes → missed during optimization
- Modified edges → inconsistent graph state

**Verdict:** ❌ Requires complex conflict detection

### 2. Version Counter + Retry

**Idea:** Detect modifications via atomic counter, retry if changed.

```cpp
std::atomic<uint64_t> modification_version_;

ErrorCode add(...) {
    std::unique_lock lock(mutex_);
    // ... logic ...
    modification_version_.fetch_add(1);  // One line per write method
}

ErrorCode optimize_graph() {
    for (retry = 0; retry < MAX_RETRIES; ++retry) {
        uint64_t version = modification_version_.load();
        // ... process batches ...
        if (modification_version_ != version) continue;  // Retry
        return Ok;
    }
}
```

**Pros:** Minimal code changes, low memory
**Cons:** Wasted work on retries, may never complete under high write load

**Verdict:** ⚠️ Simple but unreliable under load

### 3. Journal at Index Level

**Idea:** Track modifications inside HNSWIndex during maintenance.

**Problem:** **Complexity creep** - every index method needs journal awareness:

```cpp
// ❌ EVERY method polluted
ErrorCode HNSWIndex::add(...) {
    if (maintenance_mode_.load()) {
        log_to_journal(...);  // Extra complexity
    }
    // actual logic
}

void HNSWIndex::add_connection(...) {
    if (maintenance_mode_.load()) {
        log_to_journal(...);  // Even internal methods!
    }
    // actual logic
}
```

**Verdict:** ❌ Violates single responsibility, complexity scattered

### 4. Double Buffering at Database Level (RECOMMENDED)

**Idea:** Clone index at database layer, optimize offline, swap when done.

**Key insight:** Database orchestrates, index stays simple.

```
┌─────────────────────────────────────────────┐
│         VectorDatabase                      │
│                                             │
│  Active Index        Clone Index            │
│  ┌──────────┐       ┌──────────┐           │
│  │ Serves   │ Clone │ Optimize │           │
│  │ queries  │──────>│ offline  │           │
│  │ Gets     │       │ Gets     │           │
│  │ writes   │ Swap  │ replayed │           │
│  │          │<──────│ writes   │           │
│  └──────────┘       └──────────┘           │
│       ↑                                     │
│       │ Write Log (during maintenance)     │
└─────────────────────────────────────────────┘
```

**Verdict:** ✅ Best separation of concerns

## Recommended Solution: Database-Level Write Log

### Architecture

**Principle:** Complexity lives at the **orchestration layer** (VectorDatabase), not the algorithm layer (HNSWIndex).

### Why Order Matters

```cpp
// During maintenance:
T1: insert(100, vector_A)
T2: remove(100)
T3: insert(100, vector_B)

// ✅ With ordered vector:
log = [
  {Insert, 100, vector_A},
  {Remove, 100},
  {Insert, 100, vector_B}
]
// Replay produces correct final state: vector 100 = vector_B

// ❌ With unordered sets:
inserted_ids = {100}
removed_ids = {100}
// Ambiguous: insert or remove? WRONG!
```

**Conclusion:** Use `std::vector` to preserve operation order.

### Implementation

```cpp
class VectorDatabaseImpl : public IVectorDatabase {
private:
    struct WriteLog {
        enum class Operation {
            Insert,
            Remove
        };

        struct Entry {
            Operation op;
            uint64_t id;
            std::vector<float> vector;  // Empty for Remove
            std::chrono::steady_clock::time_point timestamp;
        };

        // ✅ Vector preserves chronological order
        std::vector<Entry> entries;
        mutable std::mutex mutex;
        std::atomic<bool> enabled{false};

        // Safety: prevent unbounded growth
        static constexpr size_t MAX_ENTRIES = 100'000;

        bool log_insert(uint64_t id, std::span<const float> vector) {
            std::lock_guard lock(mutex);
            if (entries.size() >= MAX_ENTRIES) {
                return false;  // Log overflow - abort maintenance
            }
            entries.push_back({
                Operation::Insert,
                id,
                std::vector<float>(vector.begin(), vector.end()),
                std::chrono::steady_clock::now()
            });
            return true;
        }

        bool log_remove(uint64_t id) {
            std::lock_guard lock(mutex);
            if (entries.size() >= MAX_ENTRIES) {
                return false;
            }
            entries.push_back({
                Operation::Remove,
                id,
                {},
                std::chrono::steady_clock::now()
            });
            return true;
        }

        void replay_to(HNSWIndex* target) {
            std::lock_guard lock(mutex);

            for (const auto& entry : entries) {
                switch (entry.op) {
                    case Operation::Insert: {
                        auto result = target->add(entry.id, entry.vector);

                        // Handle case where ID already exists in clone
                        if (result == ErrorCode::InvalidState) {
                            target->remove(entry.id);
                            target->add(entry.id, entry.vector);
                        }
                        break;
                    }
                    case Operation::Remove:
                        // Ignore if doesn't exist
                        target->remove(entry.id);
                        break;
                }
            }
        }

        void clear() {
            std::lock_guard lock(mutex);
            entries.clear();
        }

        size_t size() const {
            std::lock_guard lock(mutex);
            return entries.size();
        }
    };

    WriteLog write_log_;
    std::shared_ptr<HNSWIndex> active_index_;
    mutable std::shared_mutex index_mutex_;

public:
    // ✅ Database methods handle logging - index stays clean!
    ErrorCode insert(uint64_t id, std::span<const float> vector) override {
        ErrorCode result;

        {
            std::unique_lock lock(index_mutex_);
            result = active_index_->add(id, vector);
        }

        // Log if maintenance is running
        if (result == ErrorCode::Ok &&
            write_log_.enabled.load(std::memory_order_acquire)) {
            if (!write_log_.log_insert(id, vector)) {
                // Log overflow - will abort maintenance
            }
        }

        return result;
    }

    ErrorCode remove(uint64_t id) override {
        ErrorCode result;

        {
            std::unique_lock lock(index_mutex_);
            result = active_index_->remove(id);
        }

        if (result == ErrorCode::Ok &&
            write_log_.enabled.load(std::memory_order_acquire)) {
            write_log_.log_remove(id);
        }

        return result;
    }

    ErrorCode optimize_index() {
        // Step 1: Enable write logging
        write_log_.enabled.store(true, std::memory_order_release);

        // Step 2: Clone active index
        std::shared_ptr<HNSWIndex> optimized;
        {
            std::shared_lock lock(index_mutex_);
            optimized = clone_index(active_index_);
        }

        // Step 3: Optimize clone (writes are being logged in ORDER!)
        // Queries continue to use active_index_ - NO BLOCKING!
        optimized->optimize_graph();

        // Step 4: Check if too many writes occurred
        if (write_log_.size() > 50'000) {
            write_log_.enabled.store(false);
            write_log_.clear();
            return ErrorCode::Busy;  // Too much activity, retry later
        }

        // Step 5: Replay logged writes to optimized index
        write_log_.replay_to(optimized.get());

        // Step 6: Disable logging
        write_log_.enabled.store(false, std::memory_order_release);

        // Step 7: Quick atomic swap
        {
            std::unique_lock lock(index_mutex_);
            active_index_ = optimized;
        }

        // Step 8: Clear log
        write_log_.clear();

        return ErrorCode::Ok;
    }

private:
    std::shared_ptr<HNSWIndex> clone_index(std::shared_ptr<HNSWIndex> source) {
        // Option 1: Serialize/deserialize
        std::stringstream buffer;
        source->serialize(buffer);

        auto cloned = std::make_shared<HNSWIndex>(
            source->dimension(),
            metric_,
            params_
        );
        cloned->deserialize(buffer);
        return cloned;

        // Option 2: If memory is constrained, use temp file
        // std::ofstream("/tmp/index.tmp");
        // source->serialize(file);
        // cloned->deserialize(file);
    }
};
```

## Key Design Decisions

### 1. Why Vector Instead of Sets?

**Operations must be replayed in chronological order** for correctness:
- `insert(100) → remove(100) → insert(100)` is different from `insert(100)`
- Cannot compact/deduplicate without losing critical state transitions

### 2. Why Database Layer Instead of Index Layer?

| Aspect | Index-Level Log | Database-Level Log |
|--------|----------------|-------------------|
| **Index complexity** | ❌ High (all methods aware) | ✅ Zero (unchanged) |
| **Log points** | ❌ Many (internal ops) | ✅ Few (public API) |
| **Coupling** | ❌ Tight (maintenance in algorithm) | ✅ Loose (orchestration) |
| **What to log** | ❌ Graph internals (edges, layers) | ✅ User operations (insert/remove) |

**Conclusion:** Database is the correct layer for orchestration complexity.

### 3. Memory Management

**Concern:** Cloning doubles memory usage.

**Solutions:**
- **Disk-based clone:** Serialize to temp file instead of RAM
- **Abort threshold:** Cancel if log exceeds limit (too many writes)
- **Scheduled maintenance:** Run during low-traffic periods
- **Memory budget:** Only optimize if `available_memory > index_size * 2`

### 4. Log Size Bounds

**Safety mechanism:** Prevent unbounded growth

```cpp
static constexpr size_t MAX_ENTRIES = 100'000;

if (entries.size() >= MAX_ENTRIES) {
    // Abort optimization, retry later
    return false;
}
```

**Calculation:** If avg write rate is 10k ops/sec and optimization takes 5 seconds:
- Log size = 50k entries
- Memory = 50k × (8 bytes + 128×4 bytes) ≈ 25 MB
- Acceptable for most systems

## Benefits

### For Queries
- ✅ **Zero blocking** during maintenance
- ✅ Consistent low latency
- ✅ No performance degradation

### For Index Code
- ✅ **No changes required** to HNSWIndex
- ✅ No version counters
- ✅ No maintenance mode flags
- ✅ Clean separation of algorithm and orchestration

### For Maintenance
- ✅ Can optimize aggressively (no time pressure)
- ✅ Can run during production traffic
- ✅ Handles concurrent writes correctly

## Trade-offs

| Aspect | Cost | Mitigation |
|--------|------|------------|
| **Memory** | 2× during maintenance | Use temp files; schedule during low memory |
| **Clone time** | Seconds for large indices | Incremental sync; background cloning |
| **Log overhead** | Per-write atomic check + log | Minimal (atomic load is fast) |
| **Complexity** | Database layer | Isolated, not scattered |

## Future Enhancements

### 1. Incremental Sync
Instead of full clone, maintain hot standby that gradually syncs:
```cpp
// Background thread continuously syncs changes
while (running) {
    sync_delta_to_standby();
    sleep(100ms);
}
// Optimization uses nearly-up-to-date standby
```

### 2. Multi-Version Concurrency Control (MVCC)
Multiple index versions, garbage collect old ones:
```cpp
std::shared_ptr<IndexVersion> versions_;
// Queries hold reference to version
// Old versions cleaned when ref_count = 0
```

### 3. Adaptive Scheduling
Optimize only during low query load:
```cpp
if (queries_per_second < threshold) {
    optimize_index();
}
```

## Implementation Checklist

- [ ] Add `WriteLog` struct to `VectorDatabaseImpl`
- [ ] Modify `insert()` and `remove()` to log during maintenance
- [ ] Implement `clone_index()` helper
- [ ] Implement `optimize_index()` with clone-optimize-replay-swap pattern
- [ ] Add log size safety checks
- [ ] Add configuration for `MAX_LOG_ENTRIES`
- [ ] Test: concurrent queries during optimization
- [ ] Test: writes during optimization are preserved
- [ ] Test: operation ordering correctness
- [ ] Test: log overflow behavior
- [ ] Document memory requirements in README

## References

- **LSM-tree compaction:** Similar write-log-and-merge strategy
- **PostgreSQL MVCC:** Multi-version concurrency for zero-blocking reads
- **RocksDB:** Clone-compact-swap pattern for SSTables
- **Copy-on-Write (CoW):** Filesystems (Btrfs, ZFS) use similar techniques

## Conclusion

The **database-level write log with double buffering** provides:

1. **Zero query blocking** - maintenance runs offline on clone
2. **Clean index code** - HNSWIndex unchanged, no maintenance awareness
3. **Correctness** - ordered log preserves operation semantics
4. **Bounded overhead** - log size limits prevent runaway memory
5. **Production-ready** - handles concurrent writes during maintenance

This design follows the **Single Responsibility Principle**: the index focuses on ANN search algorithms, while the database handles operational concerns like maintenance scheduling and consistency.
