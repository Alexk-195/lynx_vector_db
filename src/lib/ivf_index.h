/**
 * @file ivf_index.h
 * @brief IVF (Inverted File Index) Implementation
 *
 * Implements the IVF algorithm for approximate nearest neighbor search.
 * IVF partitions the vector space into clusters using k-means, storing
 * vectors in inverted lists associated with each cluster centroid.
 *
 * @copyright MIT License
 */

#ifndef LYNX_IVF_INDEX_H
#define LYNX_IVF_INDEX_H

#include "../include/lynx/lynx.h"
#include "lynx_intern.h"
#include "kmeans.h"
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <limits>
#include <cstdint>
#include <cstddef>

namespace lynx {

/**
 * @brief IVF Index implementation.
 *
 * The IVF (Inverted File Index) partitions the vector space into k clusters
 * using k-means clustering. Each cluster has a centroid and an inverted list
 * storing the vectors assigned to that cluster.
 *
 * Key properties:
 * - Query complexity: O(k·D) to find centroids + O((N/k)·n_probe·D) for search
 * - Construction complexity: O(N·D·k·iters) for k-means + O(N) for assignment
 * - Memory: O(N·D) for vectors + O(k·D) for centroids
 *
 * Thread-safety: Concurrent reads are safe. Writes must be externally synchronized
 * or use the provided locking (shared_mutex).
 */
class IVFIndex : public IVectorIndex {
public:
    // -------------------------------------------------------------------------
    // Constructor and Destructor
    // -------------------------------------------------------------------------

    /**
     * @brief Construct IVF index with configuration.
     * @param dimension Vector dimensionality
     * @param metric Distance metric to use
     * @param params IVF-specific parameters
     */
    IVFIndex(std::size_t dimension, DistanceMetric metric, const IVFParams& params);

    ~IVFIndex() override = default;

    // -------------------------------------------------------------------------
    // IVectorIndex Interface Implementation
    // -------------------------------------------------------------------------

    /**
     * @brief Add a single vector to the index.
     *
     * Finds the nearest centroid and adds the vector to its inverted list.
     * Requires that centroids have been initialized (via build() or set_centroids()).
     *
     * @param id Unique identifier for the vector
     * @param vector Vector data (must match index dimension)
     * @return ErrorCode::Ok on success, error code otherwise
     */
    ErrorCode add(std::uint64_t id, std::span<const float> vector) override;

    /**
     * @brief Remove a vector from the index.
     *
     * Locates the vector using the ID-to-cluster mapping, removes it from
     * the inverted list, and updates the mapping.
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
     * @brief Search for k nearest neighbors.
     *
     * Searches the n_probe nearest clusters and returns the k nearest vectors
     * from those clusters.
     *
     * @param query Query vector
     * @param k Number of neighbors to return
     * @param params Search parameters (n_probe)
     * @return Vector of (id, distance) pairs, sorted by distance
     */
    [[nodiscard]] std::vector<SearchResultItem> search(
        std::span<const float> query,
        std::size_t k,
        const SearchParams& params) const override;

    /**
     * @brief Build index from a batch of vectors.
     *
     * Runs k-means clustering to compute centroids, assigns all vectors to
     * clusters, and builds the inverted lists. Clears any existing data.
     *
     * @param vectors Vector records to index
     * @return ErrorCode::Ok on success, error code otherwise
     */
    ErrorCode build(std::span<const VectorRecord> vectors) override;

    /**
     * @brief Serialize index to output stream.
     *
     * Saves the complete index state including metadata, centroids,
     * inverted lists, and ID-to-cluster mapping.
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

    // -------------------------------------------------------------------------
    // IVF-Specific Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Get the number of clusters (k).
     * @return Number of clusters
     */
    [[nodiscard]] std::size_t num_clusters() const { return params_.n_clusters; }

    /**
     * @brief Check if centroids have been initialized.
     * @return true if centroids exist, false otherwise
     */
    [[nodiscard]] bool has_centroids() const;

    /**
     * @brief Set centroids directly (for testing or external training).
     *
     * This allows setting pre-computed centroids without running k-means.
     * Useful for testing or when centroids are computed externally.
     *
     * @param centroids Vector of k centroids (each of size dimension_)
     * @return ErrorCode::Ok on success, error code otherwise
     */
    ErrorCode set_centroids(const std::vector<std::vector<float>>& centroids);

    /**
     * @brief Get the current centroids.
     * @return Reference to centroids vector
     */
    [[nodiscard]] const std::vector<std::vector<float>>& centroids() const;

    /**
     * @brief Get the IVF parameters.
     * @return Reference to IVFParams
     */
    [[nodiscard]] const IVFParams& params() const { return params_; }

private:
    // -------------------------------------------------------------------------
    // Internal Data Structures
    // -------------------------------------------------------------------------

    /**
     * @brief Inverted list for a single cluster.
     *
     * Stores all vectors assigned to a cluster along with their IDs.
     */
    struct InvertedList {
        std::vector<std::uint64_t> ids;           ///< Vector IDs in this cluster
        std::vector<std::vector<float>> vectors;  ///< Vector data

        /**
         * @brief Get the number of vectors in this list.
         */
        [[nodiscard]] std::size_t size() const { return ids.size(); }

        /**
         * @brief Check if the list is empty.
         */
        [[nodiscard]] bool empty() const { return ids.empty(); }
    };

    // -------------------------------------------------------------------------
    // Helper Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Find the nearest centroid to a vector.
     * @param vector Vector to find nearest centroid for
     * @return Index of the nearest centroid
     */
    [[nodiscard]] std::size_t find_nearest_centroid(std::span<const float> vector) const;

    /**
     * @brief Find the n_probe nearest centroids to a vector.
     * @param vector Vector to find nearest centroids for
     * @param n_probe Number of nearest centroids to find
     * @return Vector of cluster IDs sorted by distance (nearest first)
     */
    [[nodiscard]] std::vector<std::size_t> find_nearest_centroids(
        std::span<const float> vector,
        std::size_t n_probe) const;

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
    IVFParams params_;                                         ///< IVF configuration

    // Cluster structure
    std::vector<std::vector<float>> centroids_;               ///< k cluster centroids
    std::vector<InvertedList> inverted_lists_;                ///< k inverted lists
    std::unordered_map<std::uint64_t, std::size_t> id_to_cluster_;  ///< ID -> cluster mapping

    // Thread safety
    mutable std::shared_mutex mutex_;                          ///< Reader-writer lock

    // Constants
    static constexpr std::uint64_t kInvalidId = std::numeric_limits<std::uint64_t>::max();
};

} // namespace lynx

#endif // LYNX_IVF_INDEX_H
