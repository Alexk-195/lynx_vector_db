/**
 * @file vector_database_flat.h
 * @brief Internal implementation of the vector database
 *
 * This file contains the VectorDatabase_Impl class which provides
 * the concrete implementation of the IVectorDatabase interface.
 */

#ifndef LYNX_VECTOR_DATABASE_FLAT_H
#define LYNX_VECTOR_DATABASE_FLAT_H

#include "lynx/lynx.h"
#include <unordered_map>
#include <cstddef>
#include <cstdint>

namespace lynx {

/**
 * @brief Internal implementation of IVectorDatabase
 *
 * This class provides a basic in-memory implementation of the vector database
 * using brute-force search. It stores vectors in an unordered_map for O(1)
 * lookups and performs linear O(N) search over all vectors.
 *
 * Features:
 * - In-memory storage using std::unordered_map
 * - Brute-force search with all distance metrics
 * - Statistics tracking (queries, inserts, memory usage)
 * - Dimension validation
 * - Search filtering support
 *
 * Thread Safety: This implementation is NOT thread-safe. For concurrent
 * access, use the MPS-based implementation (planned for Phase 3).
 */
class VectorDatabase_Impl : public IVectorDatabase {
public:
    /**
     * @brief Construct a new vector database
     * @param config Database configuration
     */
    explicit VectorDatabase_Impl(const Config& config);

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
    const Config& config() const override;

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    ErrorCode flush() override;
    ErrorCode save() override;
    ErrorCode load() override;

private:
    Config config_;                                           ///< Database configuration
    std::unordered_map<std::uint64_t, VectorRecord> vectors_; ///< Vector storage
    std::size_t total_inserts_;                               ///< Total insert count
    std::size_t total_queries_;                               ///< Total query count
    double total_query_time_ms_;                              ///< Cumulative query time
};

} // namespace lynx

#endif // LYNX_VECTOR_DATABASE_FLAT_H
