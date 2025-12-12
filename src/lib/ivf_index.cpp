/**
 * @file ivf_index.cpp
 * @brief IVF (Inverted File Index) Implementation
 *
 * @copyright MIT License
 */

#include "ivf_index.h"
#include "utils.h"
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <mutex>

namespace lynx {

// ============================================================================
// Constructor
// ============================================================================

IVFIndex::IVFIndex(std::size_t dimension, DistanceMetric metric, const IVFParams& params)
    : dimension_(dimension)
    , metric_(metric)
    , params_(params)
    , centroids_()
    , inverted_lists_()
    , id_to_cluster_()
{
    if (dimension_ == 0) {
        throw std::invalid_argument("IVFIndex: dimension must be > 0");
    }

    if (params_.n_clusters == 0) {
        throw std::invalid_argument("IVFIndex: n_clusters must be > 0");
    }
}

// ============================================================================
// IVectorIndex Interface - Vector Operations
// ============================================================================

ErrorCode IVFIndex::add(std::uint64_t id, std::span<const float> vector) {
    // Validate dimension
    if (vector.size() != dimension_) {
        return ErrorCode::DimensionMismatch;
    }

    // Check if centroids have been initialized
    if (!has_centroids()) {
        return ErrorCode::InvalidState;
    }

    std::unique_lock lock(mutex_);

    // Check if ID already exists
    if (id_to_cluster_.contains(id)) {
        return ErrorCode::InvalidState;
    }

    // Find nearest centroid
    std::size_t cluster_id = find_nearest_centroid(vector);

    // Add to inverted list
    inverted_lists_[cluster_id].ids.push_back(id);
    inverted_lists_[cluster_id].vectors.push_back(
        std::vector<float>(vector.begin(), vector.end()));

    // Update ID-to-cluster mapping
    id_to_cluster_[id] = cluster_id;

    return ErrorCode::Ok;
}

ErrorCode IVFIndex::remove(std::uint64_t /*id*/) {
    // To be implemented in ticket #2004
    return ErrorCode::NotImplemented;
}

bool IVFIndex::contains(std::uint64_t id) const {
    std::shared_lock lock(mutex_);
    return id_to_cluster_.contains(id);
}

// ============================================================================
// IVectorIndex Interface - Search Operations
// ============================================================================

std::vector<SearchResultItem> IVFIndex::search(
    std::span<const float> query,
    std::size_t /*k*/,
    const SearchParams& /*params*/) const {

    // Validate dimension
    if (query.size() != dimension_) {
        return {};
    }

    // To be implemented in ticket #2003
    // For now, return empty results
    return {};
}

// ============================================================================
// IVectorIndex Interface - Batch Operations
// ============================================================================

ErrorCode IVFIndex::build(std::span<const VectorRecord> /*vectors*/) {
    // To be implemented in ticket #2004
    return ErrorCode::NotImplemented;
}

// ============================================================================
// IVectorIndex Interface - Serialization
// ============================================================================

ErrorCode IVFIndex::serialize(std::ostream& /*out*/) const {
    // To be implemented in ticket #2004
    return ErrorCode::NotImplemented;
}

ErrorCode IVFIndex::deserialize(std::istream& /*in*/) {
    // To be implemented in ticket #2004
    return ErrorCode::NotImplemented;
}

// ============================================================================
// Properties
// ============================================================================

std::size_t IVFIndex::size() const {
    std::shared_lock lock(mutex_);
    return id_to_cluster_.size();
}

std::size_t IVFIndex::dimension() const {
    return dimension_;
}

std::size_t IVFIndex::memory_usage() const {
    std::shared_lock lock(mutex_);

    std::size_t usage = 0;

    // Centroids: k * D * sizeof(float)
    usage += centroids_.size() * dimension_ * sizeof(float);

    // Inverted lists: vectors and IDs
    for (const auto& inv_list : inverted_lists_) {
        // IDs
        usage += inv_list.ids.size() * sizeof(std::uint64_t);
        // Vectors
        usage += inv_list.vectors.size() * dimension_ * sizeof(float);
    }

    // ID-to-cluster mapping (approximate)
    usage += id_to_cluster_.size() * (sizeof(std::uint64_t) + sizeof(std::size_t));

    // Add overhead for data structure bookkeeping
    usage += sizeof(IVFIndex);

    return usage;
}

// ============================================================================
// IVF-Specific Methods
// ============================================================================

bool IVFIndex::has_centroids() const {
    std::shared_lock lock(mutex_);
    return !centroids_.empty();
}

ErrorCode IVFIndex::set_centroids(const std::vector<std::vector<float>>& centroids) {
    if (centroids.empty()) {
        return ErrorCode::InvalidParameter;
    }

    // Validate all centroids have correct dimension
    for (const auto& centroid : centroids) {
        if (centroid.size() != dimension_) {
            return ErrorCode::DimensionMismatch;
        }
    }

    std::unique_lock lock(mutex_);

    // Clear existing data if any
    centroids_.clear();
    inverted_lists_.clear();
    id_to_cluster_.clear();

    // Set new centroids
    centroids_ = centroids;

    // Update params to reflect actual number of clusters
    params_.n_clusters = centroids_.size();

    // Initialize inverted lists (one per cluster)
    inverted_lists_.resize(centroids_.size());

    return ErrorCode::Ok;
}

const std::vector<std::vector<float>>& IVFIndex::centroids() const {
    std::shared_lock lock(mutex_);
    return centroids_;
}

// ============================================================================
// Helper Methods
// ============================================================================

std::size_t IVFIndex::find_nearest_centroid(std::span<const float> vector) const {
    // Note: This method is called with mutex already held

    if (centroids_.empty()) {
        return 0;
    }

    std::size_t nearest = 0;
    float min_distance = std::numeric_limits<float>::max();

    for (std::size_t i = 0; i < centroids_.size(); ++i) {
        float dist = calculate_distance(vector, centroids_[i]);
        if (dist < min_distance) {
            min_distance = dist;
            nearest = i;
        }
    }

    return nearest;
}

float IVFIndex::calculate_distance(std::span<const float> a, std::span<const float> b) const {
    return lynx::calculate_distance(a, b, metric_);
}

} // namespace lynx
