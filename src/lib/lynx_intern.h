/**
 * @file lynx_intern.h
 * @brief Lynx Vector Database - Internal Interfaces
 *
 * This header contains internal implementation details that are not part of
 * the public API. These interfaces are used by internal index implementations
 * and should not be exposed to library users.
 *
 * @copyright MIT License
 */

#ifndef LYNX_LYNX_INTERN_H
#define LYNX_LYNX_INTERN_H

#include "../include/lynx/lynx.h"

namespace lynx {

// ============================================================================
// Internal Interfaces
// ============================================================================

/**
 * @brief Abstract interface for vector index implementations.
 *
 * This interface defines the contract for all index types (HNSW, IVF, Flat).
 * Implementations are internal and not exposed in the public API.
 *
 * @note This is an internal interface. Library users should interact only
 *       with IVectorDatabase, not directly with index implementations.
 */
class IVectorIndex {
public:
    virtual ~IVectorIndex() = 0;

    // -------------------------------------------------------------------------
    // Vector Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Add a single vector to the index.
     * @param id Unique identifier for the vector
     * @param vector Vector data (must match index dimension)
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode add(std::uint64_t id, std::span<const float> vector) = 0;

    /**
     * @brief Remove a vector from the index.
     * @param id Vector identifier to remove
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode remove(std::uint64_t id) = 0;

    /**
     * @brief Check if a vector exists in the index.
     * @param id Vector identifier to check
     * @return true if vector exists, false otherwise
     */
    [[nodiscard]] virtual bool contains(std::uint64_t id) const = 0;

    // -------------------------------------------------------------------------
    // Search Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Search for k nearest neighbors.
     * @param query Query vector
     * @param k Number of neighbors to return
     * @param params Search parameters
     * @return Vector of (id, distance) pairs, sorted by distance
     */
    [[nodiscard]] virtual std::vector<SearchResultItem> search(
        std::span<const float> query,
        std::size_t k,
        const SearchParams& params) const = 0;

    // -------------------------------------------------------------------------
    // Batch Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Build index from a batch of vectors.
     * @param vectors Vector records to index
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode build(std::span<const VectorRecord> vectors) = 0;

    // -------------------------------------------------------------------------
    // Serialization
    // -------------------------------------------------------------------------

    /**
     * @brief Serialize index to output stream.
     * @param out Output stream
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode serialize(std::ostream& out) const = 0;

    /**
     * @brief Deserialize index from input stream.
     * @param in Input stream
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode deserialize(std::istream& in) = 0;

    // -------------------------------------------------------------------------
    // Properties
    // -------------------------------------------------------------------------

    /**
     * @brief Get the number of vectors in the index.
     * @return Vector count
     */
    [[nodiscard]] virtual std::size_t size() const = 0;

    /**
     * @brief Get the vector dimensionality.
     * @return Dimension
     */
    [[nodiscard]] virtual std::size_t dimension() const = 0;

    /**
     * @brief Get approximate memory usage in bytes.
     * @return Memory usage
     */
    [[nodiscard]] virtual std::size_t memory_usage() const = 0;
};

} // namespace lynx

#endif // LYNX_LYNX_INTERN_H
