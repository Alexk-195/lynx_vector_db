/**
 * @file kmeans.cpp
 * @brief K-Means Clustering Algorithm Implementation
 *
 * @copyright MIT License
 */

#include "kmeans.h"
#include "utils.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <limits>
#include <cmath>
#include <iostream>

namespace lynx {
namespace clustering {

// ============================================================================
// Constructor
// ============================================================================

KMeans::KMeans(std::size_t k, std::size_t dimension,
               DistanceMetric metric, const KMeansParams& params)
    : k_(k)
    , dimension_(dimension)
    , metric_(metric)
    , params_(params)
    , is_fitted_(false)
{
    if (k_ == 0) {
        throw std::invalid_argument("k must be greater than 0");
    }
    if (dimension_ == 0) {
        throw std::invalid_argument("dimension must be greater than 0");
    }

    // Initialize random number generator
    if (params_.random_seed.has_value()) {
        rng_.seed(params_.random_seed.value());
    } else {
        std::random_device rd;
        rng_.seed(rd());
    }
}

// ============================================================================
// Training
// ============================================================================

void KMeans::fit(std::span<const std::vector<float>> vectors) {
    if (vectors.empty()) {
        throw std::invalid_argument("Cannot fit on empty vector set");
    }

    // Validate dimensions
    for (const auto& vec : vectors) {
        if (vec.size() != dimension_) {
            throw std::invalid_argument("Vector dimension mismatch");
        }
    }

    // Adjust k if necessary (k cannot exceed number of vectors)
    std::size_t effective_k = std::min(k_, vectors.size());
    if (effective_k < k_) {
        std::cerr << "Warning: k (" << k_ << ") is greater than number of vectors ("
                  << vectors.size() << "). Reducing k to " << effective_k << std::endl;
        k_ = effective_k;
    }

    // Initialize centroids using k-means++
    initialize_centroids_plusplus(vectors);

    // Lloyd's algorithm: iterate until convergence or max iterations
    std::vector<std::size_t> assignments(vectors.size());

    for (std::size_t iter = 0; iter < params_.max_iterations; ++iter) {
        // Assignment step: assign each vector to nearest centroid
        for (std::size_t i = 0; i < vectors.size(); ++i) {
            assignments[i] = assign_to_nearest_centroid(vectors[i]);
        }

        // Save old centroids for convergence check
        auto old_centroids = centroids_;

        // Update step: recompute centroids
        update_centroids(vectors, assignments);

        // Check for convergence
        float movement = calculate_centroid_movement(old_centroids, centroids_);
        if (movement < params_.convergence_threshold) {
            break;  // Converged
        }
    }

    is_fitted_ = true;
}

// ============================================================================
// Prediction
// ============================================================================

std::vector<std::size_t> KMeans::predict(std::span<const std::vector<float>> vectors) const {
    if (!is_fitted_) {
        throw std::logic_error("KMeans::predict() called before fit()");
    }

    std::vector<std::size_t> assignments;
    assignments.reserve(vectors.size());

    for (const auto& vec : vectors) {
        if (vec.size() != dimension_) {
            throw std::invalid_argument("Vector dimension mismatch in predict()");
        }
        assignments.push_back(assign_to_nearest_centroid(vec));
    }

    return assignments;
}

// ============================================================================
// Accessors
// ============================================================================

const std::vector<std::vector<float>>& KMeans::centroids() const {
    if (!is_fitted_) {
        throw std::logic_error("KMeans::centroids() called before fit()");
    }
    return centroids_;
}

// ============================================================================
// Initialization (K-means++)
// ============================================================================

void KMeans::initialize_centroids_plusplus(std::span<const std::vector<float>> vectors) {
    centroids_.clear();
    centroids_.reserve(k_);

    // Step 1: Choose first centroid uniformly at random
    std::uniform_int_distribution<std::size_t> uniform_dist(0, vectors.size() - 1);
    std::size_t first_idx = uniform_dist(rng_);
    centroids_.push_back(vectors[first_idx]);

    // Step 2: Choose remaining k-1 centroids with probability proportional to D(x)^2
    std::vector<float> min_distances(vectors.size(), std::numeric_limits<float>::max());

    for (std::size_t c = 1; c < k_; ++c) {
        // Update minimum distances to nearest centroid
        for (std::size_t i = 0; i < vectors.size(); ++i) {
            float dist = calculate_distance(vectors[i], centroids_.back());
            min_distances[i] = std::min(min_distances[i], dist);
        }

        // Calculate D(x)^2 for probability distribution
        std::vector<float> squared_distances;
        squared_distances.reserve(vectors.size());
        for (float dist : min_distances) {
            squared_distances.push_back(dist * dist);
        }

        // Choose next centroid with probability proportional to D(x)^2
        std::discrete_distribution<std::size_t> weighted_dist(
            squared_distances.begin(), squared_distances.end());
        std::size_t next_idx = weighted_dist(rng_);
        centroids_.push_back(vectors[next_idx]);
    }
}

// ============================================================================
// Assignment
// ============================================================================

std::size_t KMeans::assign_to_nearest_centroid(std::span<const float> vector) const {
    if (centroids_.empty()) {
        throw std::logic_error("Cannot assign to nearest centroid: no centroids");
    }

    std::size_t nearest_cluster = 0;
    float min_distance = std::numeric_limits<float>::max();

    for (std::size_t c = 0; c < centroids_.size(); ++c) {
        float dist = calculate_distance(vector, centroids_[c]);
        if (dist < min_distance) {
            min_distance = dist;
            nearest_cluster = c;
        }
    }

    return nearest_cluster;
}

// ============================================================================
// Update
// ============================================================================

void KMeans::update_centroids(std::span<const std::vector<float>> vectors,
                              const std::vector<std::size_t>& assignments) {
    // Initialize new centroids and counts
    std::vector<std::vector<float>> new_centroids(k_, std::vector<float>(dimension_, 0.0f));
    std::vector<std::size_t> cluster_counts(k_, 0);

    // Accumulate vectors for each cluster
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        std::size_t cluster = assignments[i];
        cluster_counts[cluster]++;

        for (std::size_t d = 0; d < dimension_; ++d) {
            new_centroids[cluster][d] += vectors[i][d];
        }
    }

    // Compute means and handle empty clusters
    for (std::size_t c = 0; c < k_; ++c) {
        if (cluster_counts[c] > 0) {
            // Normal case: compute mean
            for (std::size_t d = 0; d < dimension_; ++d) {
                new_centroids[c][d] /= static_cast<float>(cluster_counts[c]);
            }
        } else {
            // Empty cluster: reinitialize to random vector
            std::uniform_int_distribution<std::size_t> dist(0, vectors.size() - 1);
            std::size_t random_idx = dist(rng_);
            new_centroids[c] = vectors[random_idx];
        }
    }

    centroids_ = std::move(new_centroids);
}

// ============================================================================
// Distance Calculation
// ============================================================================

float KMeans::calculate_distance(std::span<const float> a, std::span<const float> b) const {
    return utils::calculate_distance(a, b, metric_);
}

// ============================================================================
// Convergence
// ============================================================================

float KMeans::calculate_centroid_movement(
    const std::vector<std::vector<float>>& old_centroids,
    const std::vector<std::vector<float>>& new_centroids) const {

    if (old_centroids.size() != new_centroids.size()) {
        return std::numeric_limits<float>::max();
    }

    float total_movement = 0.0f;
    for (std::size_t c = 0; c < old_centroids.size(); ++c) {
        float dist = calculate_distance(old_centroids[c], new_centroids[c]);
        total_movement += dist;
    }

    return total_movement;
}

} // namespace clustering
} // namespace lynx
