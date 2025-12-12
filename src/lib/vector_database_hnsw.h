/**
 * @file vector_database_hnsw.h
 * @brief Thread-safe vector database implementation using MPS
 *
 * This implementation uses MPS (Message Processing System) for multi-threading,
 * providing concurrent read access and serialized write access to the HNSW index.
 */

#ifndef LYNX_VECTOR_DATABASE_HNSW_H
#define LYNX_VECTOR_DATABASE_HNSW_H

#include "../include/lynx/lynx.h"
#include "hnsw_index.h"
#include "mps_messages.h"
#include "mps_workers.h"
#include "write_log.h"
#include "mps.h"
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <shared_mutex>
#include <sstream>

namespace lynx {

// ============================================================================
// Simple Round-Robin Distributor
// ============================================================================

/**
 * @brief Distributes messages across multiple MPS pools using round-robin
 *
 * This class provides load balancing across multiple pools. Each pool
 * represents one thread of execution.
 */
template<typename MessageType>
class PoolDistributor {
public:
    /**
     * @brief Construct distributor with pools
     * @param pools Vector of MPS pools to distribute across
     */
    explicit PoolDistributor(std::vector<std::shared_ptr<mps::pool>> pools)
        : pools_(std::move(pools)), counter_(0) {}

    /**
     * @brief Send message to next pool in round-robin fashion
     * @param msg Message to send
     */
    void send(std::shared_ptr<MessageType> msg) {
        if (pools_.empty()) {
            throw std::runtime_error("No pools available for distribution");
        }

        size_t idx = counter_.fetch_add(1, std::memory_order_relaxed);
        pools_[idx % pools_.size()]->push_back(msg);
    }

    /**
     * @brief Get number of pools
     */
    size_t pool_count() const { return pools_.size(); }

private:
    std::vector<std::shared_ptr<mps::pool>> pools_;
    std::atomic<size_t> counter_;
};

// ============================================================================
// VectorDatabase_MPS - Thread-safe database implementation
// ============================================================================

/**
 * @brief Thread-safe vector database using MPS for multi-threading
 *
 * Architecture:
 * - N query pools (one per thread) for parallel search operations
 * - 2 index pools for concurrent write operations
 * - 1 maintenance pool for background tasks
 *
 * Each pool runs on its own thread. Messages are distributed using
 * round-robin load balancing.
 *
 * Thread Safety: Fully thread-safe via message passing.
 */
class VectorDatabase_MPS : public IVectorDatabase {
public:
    /**
     * @brief Construct thread-safe database with MPS
     * @param config Database configuration
     */
    explicit VectorDatabase_MPS(const Config& config);

    /**
     * @brief Destructor - ensures clean shutdown of all pools
     */
    ~VectorDatabase_MPS() override;

    // -------------------------------------------------------------------------
    // Single Vector Operations
    // -------------------------------------------------------------------------

    ErrorCode insert(const VectorRecord& record) override;
    ErrorCode remove(std::uint64_t id) override;
    bool contains(std::uint64_t id) const override;
    std::optional<VectorRecord> get(std::uint64_t id) const override;

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

    // -------------------------------------------------------------------------
    // Index Maintenance (Non-blocking)
    // -------------------------------------------------------------------------

    /**
     * @brief Optimize the index without blocking queries.
     *
     * This method performs index optimization using a clone-optimize-replay-swap
     * pattern that enables non-blocking maintenance:
     *
     * 1. Enable write logging to capture operations during optimization
     * 2. Clone the current index
     * 3. Optimize the clone (queries continue on active index)
     * 4. Replay logged writes to the optimized clone
     * 5. Atomically swap the active index with the optimized clone
     *
     * Benefits:
     * - Zero query blocking during maintenance
     * - Consistent low latency
     * - Handles concurrent writes correctly via ordered log replay
     *
     * @return ErrorCode::Ok on success
     * @return ErrorCode::Busy if too many writes occurred during optimization
     * @return ErrorCode::OutOfMemory if index cloning failed
     */
    ErrorCode optimize_index();

    /**
     * @brief Get the write log for inspection (mainly for testing)
     * @return Const reference to the write log
     */
    const WriteLog& get_write_log() const { return write_log_; }

private:
    /**
     * @brief Initialize MPS pools and workers
     */
    void initialize_pools();

    /**
     * @brief Shutdown all pools gracefully
     */
    void shutdown_pools();

    /**
     * @brief Clone the current index via serialization/deserialization
     * @param source The source index to clone
     * @return Shared pointer to the cloned index, nullptr on failure
     */
    std::shared_ptr<HNSWIndex> clone_index(std::shared_ptr<HNSWIndex> source);

    // Configuration
    Config config_;

    // Shared data structures
    std::shared_ptr<HNSWIndex> index_;
    std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors_;

    // Query pools (N threads for parallel reads)
    std::vector<std::shared_ptr<mps::pool>> query_pools_;
    std::unique_ptr<PoolDistributor<SearchMessage>> search_distributor_;
    std::unique_ptr<PoolDistributor<GetMessage>> get_distributor_;
    std::unique_ptr<PoolDistributor<ContainsMessage>> contains_distributor_;
    std::unique_ptr<PoolDistributor<StatsMessage>> stats_distributor_;

    // Index pools (2 threads for concurrent writes)
    std::vector<std::shared_ptr<mps::pool>> index_pools_;
    std::unique_ptr<PoolDistributor<InsertMessage>> insert_distributor_;
    std::unique_ptr<PoolDistributor<BatchInsertMessage>> batch_distributor_;
    std::unique_ptr<PoolDistributor<RemoveMessage>> remove_distributor_;

    // Maintenance pool (1 thread for background tasks)
    std::shared_ptr<mps::pool> maintenance_pool_;

    // Thread count configuration
    size_t num_query_threads_;
    size_t num_index_threads_;

    // Request tracking (mutable for const methods)
    mutable std::atomic<std::uint64_t> next_request_id_{0};

    // Statistics tracking (shared across all workers)
    std::shared_ptr<std::atomic<std::uint64_t>> total_inserts_;
    std::shared_ptr<std::atomic<std::uint64_t>> total_queries_;

    // Write log for non-blocking maintenance
    mutable WriteLog write_log_;

    // Mutex for thread-safe index swapping during maintenance
    mutable std::shared_mutex index_mutex_;
};

} // namespace lynx

#endif // LYNX_VECTOR_DATABASE_HNSW_H
