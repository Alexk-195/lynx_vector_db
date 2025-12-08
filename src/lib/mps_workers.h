/**
 * @file mps_workers.h
 * @brief MPS worker implementations for database operations
 *
 * Defines worker classes that process messages in MPS pools.
 */

#ifndef LYNX_MPS_WORKERS_H
#define LYNX_MPS_WORKERS_H

#include "mps.h"
#include "mps_messages.h"
#include "hnsw_index.h"
#include <memory>
#include <unordered_map>

namespace lynx {

// ============================================================================
// QueryWorker - Handles READ operations (concurrent, lock-free)
// ============================================================================

/**
 * @brief Worker for processing search and query operations
 *
 * QueryWorkers run in parallel across multiple pools. They perform
 * read-only operations on the HNSW index, which uses shared_mutex
 * for concurrent read access.
 *
 * Thread Safety: Multiple QueryWorkers can run concurrently.
 */
class QueryWorker : public mps::worker {
public:
    /**
     * @brief Construct a query worker
     * @param index Shared pointer to HNSW index
     * @param vectors Shared pointer to vector storage
     */
    QueryWorker(std::shared_ptr<HNSWIndex> index,
                std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors)
        : index_(index), vectors_(vectors) {}

    void process(std::shared_ptr<const mps::message> msg) override {
        try {
            // Search operation
            if (auto search_msg = std::dynamic_pointer_cast<const SearchMessage>(msg)) {
                process_search(search_msg);
                return;
            }

            // Get operation
            if (auto get_msg = std::dynamic_pointer_cast<const GetMessage>(msg)) {
                process_get(get_msg);
                return;
            }

            // Contains operation
            if (auto contains_msg = std::dynamic_pointer_cast<const ContainsMessage>(msg)) {
                process_contains(contains_msg);
                return;
            }

            // Stats operation
            if (auto stats_msg = std::dynamic_pointer_cast<const StatsMessage>(msg)) {
                process_stats(stats_msg);
                return;
            }

        } catch (const std::exception& e) {
            // Exception handling - set exception in promise
            // (we'll handle this in the specific process functions)
        }
    }

private:
    void process_search(std::shared_ptr<const SearchMessage> msg) {
        try {
            std::span<const float> query(msg->query);
            auto items = index_->search(query, msg->k, msg->params);

            // Create SearchResult
            SearchResult result;
            result.items = std::move(items);
            result.total_candidates = result.items.size();
            result.query_time_ms = 0.0; // TODO: measure actual time

            const_cast<SearchMessage*>(msg.get())->set_value(std::move(result));
        } catch (...) {
            const_cast<SearchMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_get(std::shared_ptr<const GetMessage> msg) {
        try {
            auto it = vectors_->find(msg->id);
            if (it != vectors_->end()) {
                const_cast<GetMessage*>(msg.get())->set_value(it->second);
            } else {
                const_cast<GetMessage*>(msg.get())->set_value(std::nullopt);
            }
        } catch (...) {
            const_cast<GetMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_contains(std::shared_ptr<const ContainsMessage> msg) {
        try {
            bool exists = index_->contains(msg->id);
            const_cast<ContainsMessage*>(msg.get())->set_value(exists);
        } catch (...) {
            const_cast<ContainsMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_stats(std::shared_ptr<const StatsMessage> msg) {
        try {
            DatabaseStats stats;
            stats.vector_count = index_->size();
            stats.dimension = index_->dimension();
            stats.memory_usage_bytes = index_->memory_usage();
            stats.index_memory_bytes = index_->memory_usage();
            stats.avg_query_time_ms = 0.0;
            stats.total_queries = 0;
            stats.total_inserts = 0;
            // Other stats would be gathered here
            const_cast<StatsMessage*>(msg.get())->set_value(std::move(stats));
        } catch (...) {
            const_cast<StatsMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    std::shared_ptr<HNSWIndex> index_;
    std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors_;
};

// ============================================================================
// IndexWorker - Handles WRITE operations (serialized via HNSW locks)
// ============================================================================

/**
 * @brief Worker for processing insert and delete operations
 *
 * IndexWorkers handle write operations on the HNSW index. While multiple
 * IndexWorkers can run in parallel (2 pools), the HNSW index's internal
 * shared_mutex ensures writes are properly serialized.
 *
 * Thread Safety: Multiple IndexWorkers can run, HNSW handles synchronization.
 */
class IndexWorker : public mps::worker {
public:
    /**
     * @brief Construct an index worker
     * @param index Shared pointer to HNSW index
     * @param vectors Shared pointer to vector storage
     */
    IndexWorker(std::shared_ptr<HNSWIndex> index,
                std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors)
        : index_(index), vectors_(vectors) {}

    void process(std::shared_ptr<const mps::message> msg) override {
        try {
            // Insert operation
            if (auto insert_msg = std::dynamic_pointer_cast<const InsertMessage>(msg)) {
                process_insert(insert_msg);
                return;
            }

            // Batch insert operation
            if (auto batch_msg = std::dynamic_pointer_cast<const BatchInsertMessage>(msg)) {
                process_batch_insert(batch_msg);
                return;
            }

            // Remove operation
            if (auto remove_msg = std::dynamic_pointer_cast<const RemoveMessage>(msg)) {
                process_remove(remove_msg);
                return;
            }

        } catch (const std::exception& e) {
            // Exception handling in specific process functions
        }
    }

private:
    void process_insert(std::shared_ptr<const InsertMessage> msg) {
        try {
            const auto& record = msg->record;

            // Try to add to index
            ErrorCode result = index_->add(record.id, record.vector);

            // If ID already exists, remove it first and then add
            if (result == ErrorCode::InvalidState) {
                index_->remove(record.id);
                result = index_->add(record.id, record.vector);
            }

            if (result == ErrorCode::Ok) {
                // Add/update vector storage
                (*vectors_)[record.id] = record;
            }

            const_cast<InsertMessage*>(msg.get())->set_value(result);
        } catch (...) {
            const_cast<InsertMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_batch_insert(std::shared_ptr<const BatchInsertMessage> msg) {
        try {
            const auto& records = msg->records;

            // Build index from batch
            std::span<const VectorRecord> records_span(records);
            ErrorCode result = index_->build(records_span);

            if (result == ErrorCode::Ok) {
                // Add all to vector storage
                for (const auto& record : records) {
                    (*vectors_)[record.id] = record;
                }
            }

            const_cast<BatchInsertMessage*>(msg.get())->set_value(result);
        } catch (...) {
            const_cast<BatchInsertMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_remove(std::shared_ptr<const RemoveMessage> msg) {
        try {
            // Remove from index
            ErrorCode result = index_->remove(msg->id);

            if (result == ErrorCode::Ok) {
                // Remove from vector storage
                vectors_->erase(msg->id);
            }

            const_cast<RemoveMessage*>(msg.get())->set_value(result);
        } catch (...) {
            const_cast<RemoveMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    std::shared_ptr<HNSWIndex> index_;
    std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors_;
};

// ============================================================================
// MaintenanceWorker - Handles BACKGROUND operations
// ============================================================================

/**
 * @brief Worker for background maintenance tasks
 *
 * MaintenanceWorker runs in a single pool and handles background tasks
 * like index optimization, storage compaction, and periodic statistics.
 *
 * Thread Safety: Single worker in one pool, no concurrency issues.
 */
class MaintenanceWorker : public mps::worker {
public:
    /**
     * @brief Construct a maintenance worker
     * @param index Shared pointer to HNSW index
     */
    MaintenanceWorker(std::shared_ptr<HNSWIndex> index)
        : index_(index) {}

    void process(std::shared_ptr<const mps::message> msg) override {
        try {
            // Optimize index
            if (auto optimize_msg = std::dynamic_pointer_cast<const OptimizeIndexMessage>(msg)) {
                process_optimize();
                return;
            }

            // Compact storage
            if (auto compact_msg = std::dynamic_pointer_cast<const CompactStorageMessage>(msg)) {
                process_compact();
                return;
            }

            // Flush operation
            if (auto flush_msg = std::dynamic_pointer_cast<const FlushMessage>(msg)) {
                process_flush(flush_msg);
                return;
            }

            // Save operation
            if (auto save_msg = std::dynamic_pointer_cast<const SaveMessage>(msg)) {
                process_save(save_msg);
                return;
            }

            // Load operation
            if (auto load_msg = std::dynamic_pointer_cast<const LoadMessage>(msg)) {
                process_load(load_msg);
                return;
            }

        } catch (const std::exception& e) {
            // Exception handling
        }
    }

private:
    void process_optimize() {
        // TODO: Implement index optimization
        // - Prune redundant edges
        // - Rebalance graph structure
        // - Update statistics
    }

    void process_compact() {
        // TODO: Implement storage compaction
        // - Remove deleted vectors
        // - Defragment memory
    }

    void process_flush(std::shared_ptr<const FlushMessage> msg) {
        try {
            // Currently not implemented
            const_cast<FlushMessage*>(msg.get())->set_value(ErrorCode::NotImplemented);
        } catch (...) {
            const_cast<FlushMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_save(std::shared_ptr<const SaveMessage> msg) {
        try {
            // Currently not implemented
            const_cast<SaveMessage*>(msg.get())->set_value(ErrorCode::NotImplemented);
        } catch (...) {
            const_cast<SaveMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_load(std::shared_ptr<const LoadMessage> msg) {
        try {
            // Currently not implemented
            const_cast<LoadMessage*>(msg.get())->set_value(ErrorCode::NotImplemented);
        } catch (...) {
            const_cast<LoadMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    std::shared_ptr<HNSWIndex> index_;
};

} // namespace lynx

#endif // LYNX_MPS_WORKERS_H
