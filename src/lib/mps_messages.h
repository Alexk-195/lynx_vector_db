/**
 * @file mps_messages.h
 * @brief MPS message types for database operations
 *
 * Defines all message types used for communication between the database
 * API and the MPS worker pools.
 */

#ifndef LYNX_MPS_MESSAGES_H
#define LYNX_MPS_MESSAGES_H

#include "mps.h"
#include "../include/lynx/lynx.h"
#include "hnsw_index.h"
#include <vector>
#include <future>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace lynx {

// ============================================================================
// Base Message Types
// ============================================================================

/**
 * @brief Base message for all database operations
 *
 * Contains a request ID for tracking and a promise for async response.
 */
template<typename ResultType>
struct DatabaseMessage : mps::message {
    std::uint64_t request_id{0};
    std::shared_ptr<std::promise<ResultType>> promise;

    DatabaseMessage() : promise(std::make_shared<std::promise<ResultType>>()) {}

    std::future<ResultType> get_future() {
        return promise->get_future();
    }

    void set_value(const ResultType& value) {
        promise->set_value(value);
    }

    void set_value(ResultType&& value) {
        promise->set_value(std::move(value));
    }

    void set_exception(std::exception_ptr ptr) {
        promise->set_exception(ptr);
    }
};

// ============================================================================
// Query Messages (READ operations)
// ============================================================================

/**
 * @brief Search operation message
 */
struct SearchMessage : DatabaseMessage<SearchResult> {
    std::vector<float> query;
    std::size_t k{0};
    SearchParams params;

    SearchMessage() = default;
    SearchMessage(std::span<const float> q, std::size_t k_val, const SearchParams& p = {})
        : query(q.begin(), q.end()), k(k_val), params(p) {}
};

/**
 * @brief Get vector by ID message
 */
struct GetMessage : DatabaseMessage<std::optional<VectorRecord>> {
    std::uint64_t id{0};

    GetMessage() = default;
    explicit GetMessage(std::uint64_t vec_id) : id(vec_id) {}
};

/**
 * @brief Check if vector exists message
 */
struct ContainsMessage : DatabaseMessage<bool> {
    std::uint64_t id{0};

    ContainsMessage() = default;
    explicit ContainsMessage(std::uint64_t vec_id) : id(vec_id) {}
};

/**
 * @brief Get database statistics message
 */
struct StatsMessage : DatabaseMessage<DatabaseStats> {
    StatsMessage() = default;
};

// ============================================================================
// Index Messages (WRITE operations)
// ============================================================================

/**
 * @brief Insert single vector message
 */
struct InsertMessage : DatabaseMessage<ErrorCode> {
    VectorRecord record;

    InsertMessage() = default;
    explicit InsertMessage(const VectorRecord& r) : record(r) {}
};

/**
 * @brief Batch insert message
 */
struct BatchInsertMessage : DatabaseMessage<ErrorCode> {
    std::vector<VectorRecord> records;

    BatchInsertMessage() = default;
    explicit BatchInsertMessage(std::span<const VectorRecord> recs)
        : records(recs.begin(), recs.end()) {}
};

/**
 * @brief Remove vector message
 */
struct RemoveMessage : DatabaseMessage<ErrorCode> {
    std::uint64_t id{0};

    RemoveMessage() = default;
    explicit RemoveMessage(std::uint64_t vec_id) : id(vec_id) {}
};

// ============================================================================
// Maintenance Messages (BACKGROUND operations)
// ============================================================================

/**
 * @brief Optimize index structure message
 */
struct OptimizeIndexMessage : mps::message {
    OptimizeIndexMessage() = default;
};

/**
 * @brief Compact storage message
 */
struct CompactStorageMessage : mps::message {
    CompactStorageMessage() = default;
};

// Forward declaration for WriteLog
struct WriteLog;

/**
 * @brief Non-blocking optimize index message with write log
 *
 * This message triggers a non-blocking index optimization that uses
 * a clone-optimize-replay-swap pattern. The caller provides a shared
 * reference to the WriteLog for capturing concurrent writes during
 * optimization, and the index/vectors pointers to allow atomic swapping.
 */
struct OptimizeWithLogMessage : DatabaseMessage<ErrorCode> {
    WriteLog* write_log{nullptr};
    std::shared_ptr<HNSWIndex>* active_index{nullptr};
    std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>>* vectors{nullptr};
    std::shared_mutex* index_mutex{nullptr};
    DistanceMetric metric{DistanceMetric::L2};
    HNSWParams hnsw_params{};

    OptimizeWithLogMessage() = default;
    OptimizeWithLogMessage(
        WriteLog* log,
        std::shared_ptr<HNSWIndex>* idx,
        std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>>* vecs,
        std::shared_mutex* mtx,
        DistanceMetric m,
        const HNSWParams& params)
        : write_log(log)
        , active_index(idx)
        , vectors(vecs)
        , index_mutex(mtx)
        , metric(m)
        , hnsw_params(params) {}
};

/**
 * @brief Flush data to disk message
 */
struct FlushMessage : DatabaseMessage<ErrorCode> {
    FlushMessage() = default;
};

/**
 * @brief Save database to disk message
 */
struct SaveMessage : DatabaseMessage<ErrorCode> {
    SaveMessage() = default;
};

/**
 * @brief Load database from disk message
 */
struct LoadMessage : DatabaseMessage<ErrorCode> {
    LoadMessage() = default;
};

/**
 * @brief Shutdown signal message
 */
struct ShutdownMessage : mps::message {
    ShutdownMessage() = default;
};

} // namespace lynx

#endif // LYNX_MPS_MESSAGES_H
