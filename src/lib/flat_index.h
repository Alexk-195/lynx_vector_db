/**
 * @file flat_index.h
 * @brief Flat Index (Brute-Force) Implementation
 *
 * Implements brute-force search by comparing the query vector with all
 * vectors in the database. Provides exact nearest neighbor results.
 *
 * @copyright MIT License
 */

#ifndef LYNX_FLAT_INDEX_H
#define LYNX_FLAT_INDEX_H

#include "../include/lynx/lynx.h"
#include "lynx_intern.h"
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>
#include <cstddef>

namespace lynx {

/**
 * @brief Flat Index implementation (brute-force search).
 *
 * The FlatIndex provides exact nearest neighbor search by calculating the
 * distance from the query vector to every vector in the database. While
 * this is slower than approximate methods (HNSW, IVF), it guarantees 100%
 * recall and is suitable for small datasets or validation purposes.
 *
 * Key properties:
 * - Query complexity: O(N·D) where N = number of vectors, D = dimension
 * - Construction complexity: O(1) (no index building required)
 * - Memory: O(N·D) for vector storage only
 * - Recall: 100% (exact search)
 *
 * Thread-safety: This class is thread-safe. Concurrent reads are supported
 * via std::shared_mutex. Writes are serialized.
 */
class FlatIndex : public IVectorIndex {
public:
    // -------------------------------------------------------------------------
    // Constructor and Destructor
    // -------------------------------------------------------------------------

    /**
     * @brief Construct Flat index with configuration.
     * @param dimension Vector dimensionality
     * @param metric Distance metric to use
     */
    FlatIndex(std::size_t dimension, DistanceMetric metric);

    ~FlatIndex() override = default;

    // -------------------------------------------------------------------------
    // IVectorIndex Interface Implementation
    // -------------------------------------------------------------------------

    /**
     * @brief Add a single vector to the index.
     *
     * @param id Unique identifier for the vector
     * @param vector Vector data (must match index dimension)
     * @return ErrorCode::Ok on success, error code otherwise
     */
    ErrorCode add(std::uint64_t id, std::span<const float> vector) override;

    /**
     * @brief Remove a vector from the index.
     *
     * @param id Vector identifier to remove
     * @return ErrorCode::Ok on success, ErrorCode::VectorNotFound if ID doesn't exist
     */
    ErrorCode remove(std::uint64_t id) override;

    /**
     * @brief Check if a vector exists in the index.
     * @param id Vector identifier to check
     * @return true if vector exists, false otherwise
     */
    [[nodiscard]] bool contains(std::uint64_t id) const override;

    /**
     * @brief Search for k nearest neighbors (exact search).
     *
     * Performs brute-force search by comparing the query vector with all
     * vectors in the index. This guarantees exact results (100% recall).
     *
     * @param query Query vector
     * @param k Number of neighbors to return
     * @param params Search parameters (filter function if provided)
     * @return Vector of (id, distance) pairs, sorted by distance
     */
    [[nodiscard]] std::vector<SearchResultItem> search(
        std::span<const float> query,
        std::size_t k,
        const SearchParams& params) const override;

    /**
     * @brief Build index from a batch of vectors.
     *
     * For FlatIndex, this simply clears existing data and adds all vectors.
     * No index structure is built.
     *
     * @param vectors Vector records to index
     * @return ErrorCode::Ok on success, error code otherwise
     */
    ErrorCode build(std::span<const VectorRecord> vectors) override;

    /**
     * @brief Serialize index to output stream.
     *
     * Saves the complete index state including metadata and all vectors.
     *
     * @param out Output stream
     * @return ErrorCode::Ok on success, ErrorCode::IOError on failure
     */
    ErrorCode serialize(std::ostream& out) const override;

    /**
     * @brief Deserialize index from input stream.
     *
     * Loads the index state from a stream, validating all data for integrity.
     * Requires that the dimension and metric match the index configuration.
     *
     * @param in Input stream
     * @return ErrorCode::Ok on success, error code otherwise
     */
    ErrorCode deserialize(std::istream& in) override;

    // -------------------------------------------------------------------------
    // Properties
    // -------------------------------------------------------------------------

    /**
     * @brief Get the number of vectors in the index.
     * @return Vector count
     */
    [[nodiscard]] std::size_t size() const override;

    /**
     * @brief Get the vector dimensionality.
     * @return Dimension
     */
    [[nodiscard]] std::size_t dimension() const override;

    /**
     * @brief Get approximate memory usage in bytes.
     * @return Memory usage
     */
    [[nodiscard]] std::size_t memory_usage() const override;

private:
    // -------------------------------------------------------------------------
    // Helper Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Calculate distance between two vectors.
     * @param a First vector
     * @param b Second vector
     * @return Distance according to metric_
     */
    [[nodiscard]] float calculate_distance(std::span<const float> a, std::span<const float> b) const;

    // -------------------------------------------------------------------------
    // Member Variables
    // -------------------------------------------------------------------------

    std::size_t dimension_;                                    ///< Vector dimensionality
    DistanceMetric metric_;                                    ///< Distance metric

    // Vector storage
    std::unordered_map<std::uint64_t, std::vector<float>> vectors_;  ///< ID -> vector mapping

    // Thread safety
    mutable std::shared_mutex mutex_;  ///< Reader-writer lock

    // Constants
    static constexpr std::uint32_t kMagicNumber = 0x464C4154;  ///< "FLAT" in hex
    static constexpr std::uint32_t kVersion = 1;               ///< File format version
};

} // namespace lynx

#endif // LYNX_FLAT_INDEX_H
