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
#include "write_log.h"
#include <memory>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <sstream>

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
     * @param total_queries Shared counter for total queries
     * @param total_inserts Shared counter for total inserts
     */
    QueryWorker(std::shared_ptr<HNSWIndex> index,
                std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors,
                std::shared_ptr<std::atomic<std::uint64_t>> total_queries,
                std::shared_ptr<std::atomic<std::uint64_t>> total_inserts)
        : index_(index), vectors_(vectors), total_queries_(total_queries), total_inserts_(total_inserts) {}

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
            // Increment query counter
            total_queries_->fetch_add(1, std::memory_order_relaxed);

            std::span<const float> query(msg->query);
            auto items = index_->search(query, msg->k, msg->params);

            // Create SearchResult
            SearchResult result;
            result.items = std::move(items);
            result.total_candidates = index_->size(); // Total vectors in database
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
            stats.total_queries = total_queries_->load(std::memory_order_relaxed);
            stats.total_inserts = total_inserts_->load(std::memory_order_relaxed);
            // Other stats would be gathered here
            const_cast<StatsMessage*>(msg.get())->set_value(std::move(stats));
        } catch (...) {
            const_cast<StatsMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    std::shared_ptr<HNSWIndex> index_;
    std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors_;
    std::shared_ptr<std::atomic<std::uint64_t>> total_queries_;
    std::shared_ptr<std::atomic<std::uint64_t>> total_inserts_;
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
     * @param total_inserts Shared counter for total inserts
     */
    IndexWorker(std::shared_ptr<HNSWIndex> index,
                std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors,
                std::shared_ptr<std::atomic<std::uint64_t>> total_inserts)
        : index_(index), vectors_(vectors), total_inserts_(total_inserts) {}

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
                // Increment insert counter
                total_inserts_->fetch_add(1, std::memory_order_relaxed);
            }

            const_cast<InsertMessage*>(msg.get())->set_value(result);
        } catch (...) {
            const_cast<InsertMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_batch_insert(std::shared_ptr<const BatchInsertMessage> msg) {
        try {
            const auto& records = msg->records;

            // Process records one by one, stop at first error
            for (const auto& record : records) {
                // Try to add to index
                ErrorCode result = index_->add(record.id, record.vector);

                // If ID already exists, remove it first and then add
                if (result == ErrorCode::InvalidState) {
                    index_->remove(record.id);
                    result = index_->add(record.id, record.vector);
                }

                if (result != ErrorCode::Ok) {
                    // Stop at first error, return error code
                    const_cast<BatchInsertMessage*>(msg.get())->set_value(result);
                    return;
                }

                // Add/update vector storage
                (*vectors_)[record.id] = record;
                // Increment insert counter
                total_inserts_->fetch_add(1, std::memory_order_relaxed);
            }

            // All successful
            const_cast<BatchInsertMessage*>(msg.get())->set_value(ErrorCode::Ok);
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
    std::shared_ptr<std::atomic<std::uint64_t>> total_inserts_;
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
     * @param vectors Shared pointer to vector storage
     * @param config Database configuration
     */
    MaintenanceWorker(std::shared_ptr<HNSWIndex> index,
                     std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors,
                     const Config& config)
        : index_(index), vectors_(vectors), config_(config) {}

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

            // Non-blocking optimize with write log
            if (auto optimize_log_msg = std::dynamic_pointer_cast<const OptimizeWithLogMessage>(msg)) {
                process_optimize_with_log(optimize_log_msg);
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
        // Perform graph edge pruning to improve index quality
        // This applies the RNG (Random Neighbor Graph) heuristic to remove
        // redundant edges while maintaining graph connectivity and navigability.

        // Call the HNSW index's optimize_graph method
        ErrorCode result = index_->optimize_graph();

        // In a production system, we might want to:
        // - Log the optimization results
        // - Update statistics
        // - Trigger additional maintenance tasks based on results
        // - Emit metrics for monitoring

        // For now, we silently complete the optimization
        // The operation is already thread-safe as optimize_graph() uses locks
        (void)result; // Suppress unused variable warning
    }

    void process_compact() {
        // Perform storage compaction and index integrity checks
        // This removes dangling references, validates graph consistency,
        // and ensures the entry point is valid.

        // Call the HNSW index's compact_index method
        ErrorCode result = index_->compact_index();

        // In a production system, we might want to:
        // - Log the compaction results
        // - Update statistics
        // - Emit metrics for monitoring
        // - Track how many inconsistencies were fixed

        // For now, we silently complete the compaction
        // The operation is already thread-safe as compact_index() uses locks
        (void)result; // Suppress unused variable warning
    }

    void process_optimize_with_log(std::shared_ptr<const OptimizeWithLogMessage> msg) {
        try {
            auto* write_log = msg->write_log;
            auto* active_index = msg->active_index;
            auto* index_mutex = msg->index_mutex;

            if (!write_log || !active_index || !index_mutex) {
                const_cast<OptimizeWithLogMessage*>(msg.get())->set_value(ErrorCode::InvalidParameter);
                return;
            }

            // Step 1: Enable write logging
            write_log->enabled.store(true, std::memory_order_release);

            // Step 2: Clone the active index via serialization/deserialization
            std::shared_ptr<HNSWIndex> optimized;
            {
                std::shared_lock lock(*index_mutex);
                std::stringstream buffer;

                ErrorCode err = (*active_index)->serialize(buffer);
                if (err != ErrorCode::Ok) {
                    write_log->enabled.store(false, std::memory_order_release);
                    write_log->clear();
                    const_cast<OptimizeWithLogMessage*>(msg.get())->set_value(ErrorCode::OutOfMemory);
                    return;
                }

                optimized = std::make_shared<HNSWIndex>(
                    (*active_index)->dimension(),
                    msg->metric,
                    msg->hnsw_params
                );

                err = optimized->deserialize(buffer);
                if (err != ErrorCode::Ok) {
                    write_log->enabled.store(false, std::memory_order_release);
                    write_log->clear();
                    const_cast<OptimizeWithLogMessage*>(msg.get())->set_value(ErrorCode::OutOfMemory);
                    return;
                }
            }

            // Step 3: Optimize the clone (queries continue on active index - NO BLOCKING!)
            optimized->optimize_graph();

            // Step 4: Check if too many writes occurred during optimization
            if (write_log->size() > WriteLog::kWarnThreshold) {
                write_log->enabled.store(false, std::memory_order_release);
                write_log->clear();
                const_cast<OptimizeWithLogMessage*>(msg.get())->set_value(ErrorCode::Busy);
                return;
            }

            // Step 5: Replay logged writes to optimized index
            write_log->replay_to(optimized.get());

            // Step 6: Disable logging before swap
            write_log->enabled.store(false, std::memory_order_release);

            // Step 7: Quick atomic swap
            {
                std::unique_lock lock(*index_mutex);
                *active_index = optimized;
            }

            // Step 8: Clear log
            write_log->clear();

            const_cast<OptimizeWithLogMessage*>(msg.get())->set_value(ErrorCode::Ok);

        } catch (...) {
            const_cast<OptimizeWithLogMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_flush(std::shared_ptr<const FlushMessage> msg) {
        try {
            // For now, flush is a no-op since all operations are synchronous
            // In the future, this could flush write-ahead log (WAL) buffers
            // or trigger asynchronous persistence operations

            // If WAL is enabled, we would flush the log here
            if (config_.enable_wal) {
                // TODO: Implement WAL flushing when WAL is added
                const_cast<FlushMessage*>(msg.get())->set_value(ErrorCode::NotImplemented);
                return;
            }

            // For synchronous operations, flush is always successful
            const_cast<FlushMessage*>(msg.get())->set_value(ErrorCode::Ok);

        } catch (...) {
            const_cast<FlushMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_save(std::shared_ptr<const SaveMessage> msg) {
        try {
            // Check if data_path is configured
            if (config_.data_path.empty()) {
                const_cast<SaveMessage*>(msg.get())->set_value(ErrorCode::InvalidParameter);
                return;
            }

            // Create directory if it doesn't exist
            std::filesystem::path dir_path(config_.data_path);
            if (!std::filesystem::exists(dir_path)) {
                std::filesystem::create_directories(dir_path);
            }

            // Save index to file
            std::filesystem::path index_path = dir_path / "index.hnsw";
            std::ofstream index_file(index_path, std::ios::binary);
            if (!index_file.is_open()) {
                const_cast<SaveMessage*>(msg.get())->set_value(ErrorCode::IOError);
                return;
            }

            ErrorCode result = index_->serialize(index_file);
            index_file.close();

            if (result != ErrorCode::Ok) {
                const_cast<SaveMessage*>(msg.get())->set_value(result);
                return;
            }

            // Save vector metadata to file
            std::filesystem::path metadata_path = dir_path / "metadata.dat";
            std::ofstream metadata_file(metadata_path, std::ios::binary);
            if (!metadata_file.is_open()) {
                const_cast<SaveMessage*>(msg.get())->set_value(ErrorCode::IOError);
                return;
            }

            // Write number of vectors
            size_t num_vectors = vectors_->size();
            metadata_file.write(reinterpret_cast<const char*>(&num_vectors), sizeof(num_vectors));

            // Write each vector's metadata
            for (const auto& [id, record] : *vectors_) {
                // Write ID
                metadata_file.write(reinterpret_cast<const char*>(&id), sizeof(id));

                // Write metadata string (if present)
                bool has_metadata = record.metadata.has_value();
                metadata_file.write(reinterpret_cast<const char*>(&has_metadata), sizeof(has_metadata));

                if (has_metadata) {
                    size_t metadata_len = record.metadata->size();
                    metadata_file.write(reinterpret_cast<const char*>(&metadata_len), sizeof(metadata_len));
                    metadata_file.write(record.metadata->data(), metadata_len);
                }
            }

            metadata_file.close();

            if (!metadata_file.good()) {
                const_cast<SaveMessage*>(msg.get())->set_value(ErrorCode::IOError);
                return;
            }

            const_cast<SaveMessage*>(msg.get())->set_value(ErrorCode::Ok);

        } catch (...) {
            const_cast<SaveMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    void process_load(std::shared_ptr<const LoadMessage> msg) {
        try {
            // Check if data_path is configured
            if (config_.data_path.empty()) {
                const_cast<LoadMessage*>(msg.get())->set_value(ErrorCode::InvalidParameter);
                return;
            }

            std::filesystem::path dir_path(config_.data_path);
            if (!std::filesystem::exists(dir_path)) {
                const_cast<LoadMessage*>(msg.get())->set_value(ErrorCode::IOError);
                return;
            }

            // Load index from file
            std::filesystem::path index_path = dir_path / "index.hnsw";
            if (!std::filesystem::exists(index_path)) {
                const_cast<LoadMessage*>(msg.get())->set_value(ErrorCode::IOError);
                return;
            }

            std::ifstream index_file(index_path, std::ios::binary);
            if (!index_file.is_open()) {
                const_cast<LoadMessage*>(msg.get())->set_value(ErrorCode::IOError);
                return;
            }

            ErrorCode result = index_->deserialize(index_file);
            index_file.close();

            if (result != ErrorCode::Ok) {
                const_cast<LoadMessage*>(msg.get())->set_value(result);
                return;
            }

            // Load vector metadata from file
            std::filesystem::path metadata_path = dir_path / "metadata.dat";
            if (!std::filesystem::exists(metadata_path)) {
                const_cast<LoadMessage*>(msg.get())->set_value(ErrorCode::IOError);
                return;
            }

            std::ifstream metadata_file(metadata_path, std::ios::binary);
            if (!metadata_file.is_open()) {
                const_cast<LoadMessage*>(msg.get())->set_value(ErrorCode::IOError);
                return;
            }

            // Read number of vectors
            size_t num_vectors;
            metadata_file.read(reinterpret_cast<char*>(&num_vectors), sizeof(num_vectors));

            // Clear existing metadata
            vectors_->clear();

            // Read each vector's metadata
            for (size_t i = 0; i < num_vectors; ++i) {
                // Read ID
                uint64_t id;
                metadata_file.read(reinterpret_cast<char*>(&id), sizeof(id));

                // Get vector data from index (the index already loaded the vectors)
                VectorRecord record;
                record.id = id;

                // We need to get the vector from the index
                // For now, create an empty vector as placeholder
                // The actual vector data is in the index
                record.vector.resize(index_->dimension());
                // Note: This is a simplification. In a production system,
                // we might want to store vectors separately or have a way
                // to retrieve them from the index.

                // Read metadata string (if present)
                bool has_metadata;
                metadata_file.read(reinterpret_cast<char*>(&has_metadata), sizeof(has_metadata));

                if (has_metadata) {
                    size_t metadata_len;
                    metadata_file.read(reinterpret_cast<char*>(&metadata_len), sizeof(metadata_len));

                    std::string metadata_str(metadata_len, '\0');
                    metadata_file.read(&metadata_str[0], metadata_len);
                    record.metadata = metadata_str;
                }

                (*vectors_)[id] = std::move(record);
            }

            metadata_file.close();

            if (!metadata_file.good()) {
                const_cast<LoadMessage*>(msg.get())->set_value(ErrorCode::IOError);
                return;
            }

            const_cast<LoadMessage*>(msg.get())->set_value(ErrorCode::Ok);

        } catch (...) {
            const_cast<LoadMessage*>(msg.get())->set_exception(std::current_exception());
        }
    }

    std::shared_ptr<HNSWIndex> index_;
    std::shared_ptr<std::unordered_map<std::uint64_t, VectorRecord>> vectors_;
    Config config_;
};

} // namespace lynx

#endif // LYNX_MPS_WORKERS_H
