/**
 * @file vector_database.h
 * @brief Unified vector database implementation for all index types
 *
 * This implementation uses the delegation pattern to support multiple
 * index types (Flat, HNSW, IVF) through a common IVectorIndex interface.
 *
 * Thread Safety:
 * - Uses std::shared_mutex for readers-writer lock pattern
 * - Multiple concurrent readers (search, get, contains, all_records, stats)
 * - Exclusive writer access (insert, remove, batch_insert, save, load)
 *
 * @copyright MIT License
 */

#ifndef LYNX_VECTOR_DATABASE_H
#define LYNX_VECTOR_DATABASE_H

#include "../include/lynx/lynx.h"
#include "lynx_intern.h"
#include "record_iterator_impl.h"
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include <shared_mutex>

namespace lynx {

/**
 * @brief Unified vector database implementation.
 *
 * This class provides a single implementation of IVectorDatabase
 * that works with any IVectorIndex implementation (Flat, HNSW, IVF).
 *
 * Features:
 * - In-memory vector storage
 * - Delegates search operations to pluggable index implementations
 * - Statistics tracking (queries, inserts, memory usage)
 * - Persistence support (save/load)
 *
 * Thread Safety:
 * - Thread-safe using std::shared_mutex (readers-writer lock)
 * - Read operations use shared locks (concurrent reads allowed)
 * - Write operations use exclusive locks (serialized writes)
 * - Statistics use atomic operations for lock-free updates
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

    // Vector storage
    std::unordered_map<std::uint64_t, VectorRecord> vectors_; ///< Vector storage

    // Thread safety
    mutable std::shared_mutex vectors_mutex_;                 ///< Protects vectors_ map

    // Statistics (using atomics for lock-free updates)
    // Marked mutable to allow updates in const methods (search, stats)
    mutable std::atomic<std::size_t> total_inserts_{0};               ///< Total insert count
    mutable std::atomic<std::size_t> total_queries_{0};               ///< Total query count
    mutable std::atomic<double> total_query_time_ms_{0.0};            ///< Cumulative query time

    // Constants for persistence
    static constexpr std::uint32_t kMagicNumber = 0x4C594E58;  ///< "LYNX" in hex
    static constexpr std::uint32_t kVersion = 1;               ///< File format version
};

} // namespace lynx

#endif // LYNX_VECTOR_DATABASE_H
