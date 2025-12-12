/**
 * @file kmeans.h
 * @brief K-Means Clustering Algorithm Implementation
 *
 * Implements k-means clustering with k-means++ initialization for better
 * centroid placement. Used as a foundation for IVF (Inverted File Index).
 *
 * @copyright MIT License
 */

#ifndef LYNX_KMEANS_H
#define LYNX_KMEANS_H

#include "../include/lynx/lynx.h"
#include <vector>
#include <span>
#include <random>
#include <optional>
#include <cstddef>
#include <cstdint>

namespace lynx {
namespace clustering {

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Configuration parameters for k-means clustering.
 */
struct KMeansParams {
    std::size_t max_iterations = 100;           ///< Maximum iterations for Lloyd's algorithm
    float convergence_threshold = 1e-4f;        ///< Convergence threshold for centroid movement
    std::optional<std::uint64_t> random_seed = std::nullopt;  ///< Random seed (nullopt = non-deterministic)
};

// ============================================================================
// K-Means Class
// ============================================================================

/**
 * @brief K-Means clustering algorithm with k-means++ initialization.
 *
 * Partitions vectors into k clusters by iteratively:
 * 1. Assigning each vector to its nearest centroid
 * 2. Updating centroids as the mean of assigned vectors
 * 3. Repeating until convergence or max iterations reached
 *
 * Key features:
 * - K-means++ initialization for better initial centroids
 * - Support for L2, Cosine, and DotProduct distance metrics
 * - Handles edge cases (k > N, empty clusters, etc.)
 * - Configurable convergence criteria
 *
 * Thread-safety: Not thread-safe. External synchronization required.
 */
class KMeans {
public:
    // -------------------------------------------------------------------------
    // Constructor
    // -------------------------------------------------------------------------

    /**
     * @brief Construct k-means clustering with configuration.
     * @param k Number of clusters
     * @param dimension Vector dimensionality
     * @param metric Distance metric to use
     * @param params K-means parameters (iterations, convergence, seed)
     */
    KMeans(std::size_t k, std::size_t dimension,
           DistanceMetric metric, const KMeansParams& params = {});

    // -------------------------------------------------------------------------
    // Training
    // -------------------------------------------------------------------------

    /**
     * @brief Fit k-means on training vectors.
     *
     * Runs k-means++ initialization followed by Lloyd's algorithm to find
     * k cluster centroids. After calling fit(), centroids are available
     * via centroids() method.
     *
     * @param vectors Training vectors (must all have dimension_ size)
     * @throws std::invalid_argument if vectors is empty or has wrong dimension
     */
    void fit(std::span<const std::vector<float>> vectors);

    // -------------------------------------------------------------------------
    // Prediction
    // -------------------------------------------------------------------------

    /**
     * @brief Predict cluster assignments for vectors.
     *
     * Assigns each vector to its nearest centroid. Must call fit() first.
     *
     * @param vectors Vectors to assign to clusters
     * @return Vector of cluster IDs [0, k-1] for each input vector
     * @throws std::logic_error if fit() hasn't been called yet
     * @throws std::invalid_argument if vectors have wrong dimension
     */
    [[nodiscard]] std::vector<std::size_t> predict(std::span<const std::vector<float>> vectors) const;

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /**
     * @brief Get the cluster centroids.
     * @return Vector of k centroids (each of size dimension_)
     * @throws std::logic_error if fit() hasn't been called yet
     */
    [[nodiscard]] const std::vector<std::vector<float>>& centroids() const;

    /**
     * @brief Check if the model has been fitted.
     * @return true if fit() has been called, false otherwise
     */
    [[nodiscard]] bool is_fitted() const { return is_fitted_; }

    /**
     * @brief Get the number of clusters.
     * @return Number of clusters (k)
     */
    [[nodiscard]] std::size_t k() const { return k_; }

    /**
     * @brief Get vector dimensionality.
     * @return Dimension
     */
    [[nodiscard]] std::size_t dimension() const { return dimension_; }

private:
    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize centroids using k-means++ algorithm.
     *
     * K-means++ initialization:
     * 1. Choose first centroid uniformly at random
     * 2. For each subsequent centroid:
     *    - Calculate D(x)^2 for each point x (distance to nearest centroid)
     *    - Choose next centroid with probability proportional to D(x)^2
     *
     * This initialization provides better starting points than random,
     * leading to faster convergence and better final clusters.
     *
     * @param vectors Training vectors
     */
    void initialize_centroids_plusplus(std::span<const std::vector<float>> vectors);

    // -------------------------------------------------------------------------
    // Assignment
    // -------------------------------------------------------------------------

    /**
     * @brief Assign a vector to its nearest centroid.
     *
     * @param vector Vector to assign
     * @return Cluster ID [0, k-1] of nearest centroid
     */
    [[nodiscard]] std::size_t assign_to_nearest_centroid(std::span<const float> vector) const;

    // -------------------------------------------------------------------------
    // Update
    // -------------------------------------------------------------------------

    /**
     * @brief Update centroids as the mean of assigned vectors.
     *
     * Recomputes each centroid as the mean of all vectors assigned to it.
     * Handles empty clusters by reinitializing them to random vectors.
     *
     * @param vectors Training vectors
     * @param assignments Cluster assignments for each vector
     */
    void update_centroids(std::span<const std::vector<float>> vectors,
                         const std::vector<std::size_t>& assignments);

    // -------------------------------------------------------------------------
    // Distance Calculation
    // -------------------------------------------------------------------------

    /**
     * @brief Calculate distance between two vectors.
     *
     * @param a First vector
     * @param b Second vector
     * @return Distance according to metric_
     */
    [[nodiscard]] float calculate_distance(std::span<const float> a, std::span<const float> b) const;

    // -------------------------------------------------------------------------
    // Convergence
    // -------------------------------------------------------------------------

    /**
     * @brief Calculate total centroid movement (for convergence detection).
     *
     * Computes sum of distances between old and new centroids.
     *
     * @param old_centroids Previous centroids
     * @param new_centroids Updated centroids
     * @return Total movement distance
     */
    [[nodiscard]] float calculate_centroid_movement(
        const std::vector<std::vector<float>>& old_centroids,
        const std::vector<std::vector<float>>& new_centroids) const;

    // -------------------------------------------------------------------------
    // Member Variables
    // -------------------------------------------------------------------------

    std::size_t k_;                              ///< Number of clusters
    std::size_t dimension_;                      ///< Vector dimensionality
    DistanceMetric metric_;                      ///< Distance metric
    KMeansParams params_;                        ///< Algorithm parameters

    std::vector<std::vector<float>> centroids_;  ///< Cluster centroids (k x dimension)
    bool is_fitted_;                             ///< Whether fit() has been called

    // Random number generation
    std::mt19937_64 rng_;                        ///< Random number generator
};

} // namespace clustering
} // namespace lynx

#endif // LYNX_KMEANS_H
