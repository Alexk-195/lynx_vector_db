/**
 * @file vector_database_ivf.h
 * @brief Vector database implementation using IVF index
 *
 * This file contains the VectorDatabase_IVF class which provides
 * a concrete implementation of the IVectorDatabase interface using
 * the IVF (Inverted File Index) for efficient approximate nearest
 * neighbor search.
 */

#ifndef LYNX_VECTOR_DATABASE_IVF_H
#define LYNX_VECTOR_DATABASE_IVF_H

#include "../include/lynx/lynx.h"
#include "ivf_index.h"
#include <unordered_map>
#include <memory>
#include <cstddef>
#include <cstdint>

namespace lynx {

/**
 * @brief Vector database implementation using IVF index
 *
 * This class provides an in-memory implementation of the vector database
 * using the IVF (Inverted File Index) algorithm for approximate nearest
 * neighbor search. IVF partitions the vector space into clusters, enabling
 * faster searches with tunable recall via the n_probe parameter.
 *
 * Features:
 * - In-memory storage using std::unordered_map
 * - IVF index for fast approximate search
 * - Statistics tracking (queries, inserts, memory usage)
 * - Dimension validation
 * - Thread-safe (via IVFIndex's internal shared_mutex)
 *
 * Performance Characteristics:
 * - Construction: O(N·D·k·iters) for k-means + O(N) for assignment
 * - Query: O(k·D) to find centroids + O((N/k)·n_probe·D) for search
 * - Memory: O(N·D) for vectors + O(k·D) for centroids
 *
 * Thread Safety: This implementation is thread-safe for concurrent reads.
 * Writes should be externally synchronized or use the MPS-based variant.
 */
class VectorDatabase_IVF : public IVectorDatabase {
public:
    /**
     * @brief Construct a new IVF-based vector database
     * @param config Database configuration (must specify IVF parameters)
     */
    explicit VectorDatabase_IVF(const Config& config);

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
    const Config& config() const override;

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    ErrorCode flush() override;
    ErrorCode save() override;
    ErrorCode load() override;

private:
    Config config_;                                           ///< Database configuration
    std::shared_ptr<IVFIndex> index_;                        ///< IVF index
    std::unordered_map<std::uint64_t, VectorRecord> vectors_; ///< Vector storage
    std::size_t total_inserts_;                               ///< Total insert count
    std::size_t total_queries_;                               ///< Total query count
    mutable double total_query_time_ms_;                      ///< Cumulative query time
};

} // namespace lynx

#endif // LYNX_VECTOR_DATABASE_IVF_H
