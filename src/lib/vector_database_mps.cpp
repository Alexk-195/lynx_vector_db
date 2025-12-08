/**
 * @file vector_database_mps.cpp
 * @brief Implementation of thread-safe vector database using MPS
 */

#include "vector_database_mps.h"
#include <stdexcept>
#include <algorithm>

namespace lynx {

// ============================================================================
// Constructor and Destructor
// ============================================================================

VectorDatabase_MPS::VectorDatabase_MPS(const Config& config)
    : config_(config),
      vectors_(std::make_shared<std::unordered_map<std::uint64_t, VectorRecord>>()),
      total_inserts_(std::make_shared<std::atomic<std::uint64_t>>(0)),
      total_queries_(std::make_shared<std::atomic<std::uint64_t>>(0)) {

    // Determine thread counts
    num_query_threads_ = config_.num_query_threads > 0
        ? config_.num_query_threads
        : std::thread::hardware_concurrency();

    if (num_query_threads_ == 0) {
        num_query_threads_ = 4; // Default fallback
    }

    num_index_threads_ = config_.num_index_threads > 0
        ? config_.num_index_threads
        : 2; // Default: 2 index threads

    // Create HNSW index using config's HNSW parameters
    index_ = std::make_shared<HNSWIndex>(
        config_.dimension,
        config_.distance_metric,
        config_.hnsw_params
    );

    // Initialize all pools and workers
    initialize_pools();
}

VectorDatabase_MPS::~VectorDatabase_MPS() {
    shutdown_pools();
}

// ============================================================================
// Pool Management
// ============================================================================

void VectorDatabase_MPS::initialize_pools() {
    // ------------------------------------------------------------------------
    // Create Query Pools (N threads for parallel reads)
    // ------------------------------------------------------------------------
    for (size_t i = 0; i < num_query_threads_; ++i) {
        auto pool = mps::pool::create();
        pool->node_name("QueryPool_" + std::to_string(i));

        // Add query worker to this pool with statistics counters
        auto worker = std::make_shared<QueryWorker>(index_, vectors_, total_queries_, total_inserts_);
        pool->add_worker(worker);

        pool->start();
        query_pools_.push_back(pool);
    }

    // Create distributors for query operations
    search_distributor_ = std::make_unique<PoolDistributor<SearchMessage>>(query_pools_);
    get_distributor_ = std::make_unique<PoolDistributor<GetMessage>>(query_pools_);
    contains_distributor_ = std::make_unique<PoolDistributor<ContainsMessage>>(query_pools_);
    stats_distributor_ = std::make_unique<PoolDistributor<StatsMessage>>(query_pools_);

    // ------------------------------------------------------------------------
    // Create Index Pools (2 threads for concurrent writes)
    // ------------------------------------------------------------------------
    for (size_t i = 0; i < num_index_threads_; ++i) {
        auto pool = mps::pool::create();
        pool->node_name("IndexPool_" + std::to_string(i));

        // Add index worker to this pool with statistics counter
        auto worker = std::make_shared<IndexWorker>(index_, vectors_, total_inserts_);
        pool->add_worker(worker);

        pool->start();
        index_pools_.push_back(pool);
    }

    // Create distributors for index operations
    insert_distributor_ = std::make_unique<PoolDistributor<InsertMessage>>(index_pools_);
    batch_distributor_ = std::make_unique<PoolDistributor<BatchInsertMessage>>(index_pools_);
    remove_distributor_ = std::make_unique<PoolDistributor<RemoveMessage>>(index_pools_);

    // ------------------------------------------------------------------------
    // Create Maintenance Pool (1 thread for background tasks)
    // ------------------------------------------------------------------------
    maintenance_pool_ = mps::pool::create();
    maintenance_pool_->node_name("MaintenancePool");

    auto maintenance_worker = std::make_shared<MaintenanceWorker>(index_);
    maintenance_pool_->add_worker(maintenance_worker);

    maintenance_pool_->start();
}

void VectorDatabase_MPS::shutdown_pools() {
    // Stop all query pools
    for (auto& pool : query_pools_) {
        pool->stop();
    }

    // Stop all index pools
    for (auto& pool : index_pools_) {
        pool->stop();
    }

    // Stop maintenance pool
    if (maintenance_pool_) {
        maintenance_pool_->stop();
    }

    // Wait for all query pools to finish
    for (auto& pool : query_pools_) {
        pool->join();
    }

    // Wait for all index pools to finish
    for (auto& pool : index_pools_) {
        pool->join();
    }

    // Wait for maintenance pool
    if (maintenance_pool_) {
        maintenance_pool_->join();
    }
}

// ============================================================================
// Single Vector Operations
// ============================================================================

ErrorCode VectorDatabase_MPS::insert(const VectorRecord& record) {
    // Validate dimension
    if (record.vector.size() != config_.dimension) {
        return ErrorCode::DimensionMismatch;
    }

    // Create insert message
    auto msg = std::make_shared<InsertMessage>(record);
    msg->request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

    // Get future before sending
    auto future = msg->get_future();

    // Distribute to index pool
    insert_distributor_->send(msg);

    // Wait for result
    return future.get();
}

ErrorCode VectorDatabase_MPS::remove(std::uint64_t id) {
    // Create remove message
    auto msg = std::make_shared<RemoveMessage>(id);
    msg->request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

    // Get future before sending
    auto future = msg->get_future();

    // Distribute to index pool
    remove_distributor_->send(msg);

    // Wait for result
    return future.get();
}

bool VectorDatabase_MPS::contains(std::uint64_t id) const {
    // Create contains message
    auto msg = std::make_shared<ContainsMessage>(id);
    msg->request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

    // Get future before sending
    auto future = msg->get_future();

    // Distribute to query pool
    const_cast<VectorDatabase_MPS*>(this)->contains_distributor_->send(msg);

    // Wait for result
    return future.get();
}

std::optional<VectorRecord> VectorDatabase_MPS::get(std::uint64_t id) const {
    // Create get message
    auto msg = std::make_shared<GetMessage>(id);
    msg->request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

    // Get future before sending
    auto future = msg->get_future();

    // Distribute to query pool
    const_cast<VectorDatabase_MPS*>(this)->get_distributor_->send(msg);

    // Wait for result
    return future.get();
}

// ============================================================================
// Search Operations
// ============================================================================

SearchResult VectorDatabase_MPS::search(std::span<const float> query, std::size_t k) const {
    SearchParams default_params;
    default_params.ef_search = config_.hnsw_params.ef_search;
    return search(query, k, default_params);
}

SearchResult VectorDatabase_MPS::search(std::span<const float> query, std::size_t k,
                                        const SearchParams& params) const {
    // Validate dimension
    if (query.size() != config_.dimension) {
        SearchResult result;
        result.items.clear();
        result.total_candidates = 0;
        result.query_time_ms = 0.0;
        return result;
    }

    // Create search message
    auto msg = std::make_shared<SearchMessage>(query, k, params);
    msg->request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

    // Get future before sending
    auto future = msg->get_future();

    // Distribute to query pool
    const_cast<VectorDatabase_MPS*>(this)->search_distributor_->send(msg);

    // Wait for result
    return future.get();
}

// ============================================================================
// Batch Operations
// ============================================================================

ErrorCode VectorDatabase_MPS::batch_insert(std::span<const VectorRecord> records) {
    // Don't validate upfront - let the worker process one by one
    // and stop at the first error (keeping previously inserted records)

    // Create batch insert message
    auto msg = std::make_shared<BatchInsertMessage>(records);
    msg->request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

    // Get future before sending
    auto future = msg->get_future();

    // Distribute to index pool
    batch_distributor_->send(msg);

    // Wait for result
    return future.get();
}

// ============================================================================
// Database Properties
// ============================================================================

std::size_t VectorDatabase_MPS::size() const {
    return index_->size();
}

std::size_t VectorDatabase_MPS::dimension() const {
    return config_.dimension;
}

DatabaseStats VectorDatabase_MPS::stats() const {
    // Create stats message
    auto msg = std::make_shared<StatsMessage>();
    msg->request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);

    // Get future before sending
    auto future = msg->get_future();

    // Distribute to query pool
    const_cast<VectorDatabase_MPS*>(this)->stats_distributor_->send(msg);

    // Wait for result
    return future.get();
}

// ============================================================================
// Persistence
// ============================================================================

ErrorCode VectorDatabase_MPS::flush() {
    auto msg = std::make_shared<FlushMessage>();
    auto future = msg->get_future();
    maintenance_pool_->push_back(msg);
    return future.get();
}

ErrorCode VectorDatabase_MPS::save() {
    auto msg = std::make_shared<SaveMessage>();
    auto future = msg->get_future();
    maintenance_pool_->push_back(msg);
    return future.get();
}

ErrorCode VectorDatabase_MPS::load() {
    auto msg = std::make_shared<LoadMessage>();
    auto future = msg->get_future();
    maintenance_pool_->push_back(msg);
    return future.get();
}

} // namespace lynx
