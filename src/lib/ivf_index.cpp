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
#include <istream>
#include <ostream>
#include <string>

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

ErrorCode IVFIndex::remove(std::uint64_t id) {
    std::unique_lock lock(mutex_);

    // Find which cluster contains this ID
    auto it = id_to_cluster_.find(id);
    if (it == id_to_cluster_.end()) {
        return ErrorCode::VectorNotFound;
    }

    std::size_t cluster_id = it->second;
    auto& inv_list = inverted_lists_[cluster_id];

    // Find position in inverted list
    auto id_it = std::find(inv_list.ids.begin(), inv_list.ids.end(), id);
    if (id_it == inv_list.ids.end()) {
        return ErrorCode::InvalidState;  // Inconsistent state
    }

    std::size_t pos = std::distance(inv_list.ids.begin(), id_it);

    // Remove from inverted list (swap with last for O(1) removal)
    if (pos != inv_list.ids.size() - 1) {
        std::swap(inv_list.ids[pos], inv_list.ids.back());
        std::swap(inv_list.vectors[pos], inv_list.vectors.back());
    }
    inv_list.ids.pop_back();
    inv_list.vectors.pop_back();

    // Remove from mapping
    id_to_cluster_.erase(it);

    return ErrorCode::Ok;
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
    std::size_t k,
    const SearchParams& params) const {

    // Validate dimension
    if (query.size() != dimension_) {
        return {};
    }

    std::shared_lock lock(mutex_);

    // Check if centroids have been initialized
    if (centroids_.empty()) {
        return {};
    }

    // If index is empty, return empty results
    if (id_to_cluster_.empty()) {
        return {};
    }

    // Get n_probe from params, default to IVFParams.n_probe
    std::size_t n_probe = params.n_probe;

    // Clamp n_probe to valid range [1, num_clusters]
    n_probe = std::max(std::size_t{1}, std::min(n_probe, centroids_.size()));

    // Step 1: Find n_probe nearest centroids
    std::vector<std::size_t> probe_clusters = find_nearest_centroids(query, n_probe);

    // Step 2: Search within selected clusters and collect candidates
    std::vector<SearchResultItem> candidates;

    for (std::size_t cluster_id : probe_clusters) {
        const auto& inv_list = inverted_lists_[cluster_id];

        // Skip empty clusters
        if (inv_list.empty()) {
            continue;
        }

        // Calculate distance to each vector in this cluster
        for (std::size_t i = 0; i < inv_list.ids.size(); ++i) {
            float dist = calculate_distance(query, inv_list.vectors[i]);
            candidates.push_back({inv_list.ids[i], dist});
        }
    }

    // Step 3: Select top-k results
    // Use partial_sort for efficiency (only sort what we need)
    std::size_t result_size = std::min(k, candidates.size());

    if (result_size == 0) {
        return {};
    }

    std::partial_sort(
        candidates.begin(),
        candidates.begin() + result_size,
        candidates.end(),
        [](const SearchResultItem& a, const SearchResultItem& b) {
            return a.distance < b.distance;
        });

    // Resize to k (or less if we don't have enough candidates)
    candidates.resize(result_size);

    return candidates;
}

// ============================================================================
// IVectorIndex Interface - Batch Operations
// ============================================================================

ErrorCode IVFIndex::build(std::span<const VectorRecord> vectors) {
    if (vectors.empty()) {
        return ErrorCode::InvalidParameter;
    }

    // Validate all vectors have correct dimension
    for (const auto& rec : vectors) {
        if (rec.vector.size() != dimension_) {
            return ErrorCode::DimensionMismatch;
        }
    }

    std::unique_lock lock(mutex_);

    // Clear existing data
    inverted_lists_.clear();
    centroids_.clear();
    id_to_cluster_.clear();

    // Extract vector data for k-means
    std::vector<std::vector<float>> vec_data;
    vec_data.reserve(vectors.size());
    for (const auto& rec : vectors) {
        vec_data.push_back(rec.vector);
    }

    // Run k-means clustering
    clustering::KMeans kmeans(params_.n_clusters, dimension_, metric_, {});
    kmeans.fit(vec_data);
    centroids_ = kmeans.centroids();

    // Initialize inverted lists
    inverted_lists_.resize(centroids_.size());

    // Assign vectors to clusters
    auto assignments = kmeans.predict(vec_data);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        std::size_t cluster_id = assignments[i];
        inverted_lists_[cluster_id].ids.push_back(vectors[i].id);
        inverted_lists_[cluster_id].vectors.push_back(vectors[i].vector);
        id_to_cluster_[vectors[i].id] = cluster_id;
    }

    return ErrorCode::Ok;
}

// ============================================================================
// IVectorIndex Interface - Serialization
// ============================================================================

ErrorCode IVFIndex::serialize(std::ostream& out) const {
    std::shared_lock lock(mutex_);

    // Write header
    out.write("IVFX", 4);
    std::uint32_t version = 1;
    out.write(reinterpret_cast<const char*>(&version), sizeof(version));

    std::uint64_t dim = dimension_;
    out.write(reinterpret_cast<const char*>(&dim), sizeof(dim));

    std::uint32_t metric = static_cast<std::uint32_t>(metric_);
    out.write(reinterpret_cast<const char*>(&metric), sizeof(metric));

    // Write centroids
    std::uint64_t num_clusters = centroids_.size();
    out.write(reinterpret_cast<const char*>(&num_clusters), sizeof(num_clusters));

    for (const auto& centroid : centroids_) {
        out.write(reinterpret_cast<const char*>(centroid.data()),
                 dimension_ * sizeof(float));
    }

    // Write inverted lists
    for (const auto& inv_list : inverted_lists_) {
        std::uint64_t list_size = inv_list.ids.size();
        out.write(reinterpret_cast<const char*>(&list_size), sizeof(list_size));

        if (list_size > 0) {
            out.write(reinterpret_cast<const char*>(inv_list.ids.data()),
                     list_size * sizeof(std::uint64_t));

            for (const auto& vec : inv_list.vectors) {
                out.write(reinterpret_cast<const char*>(vec.data()),
                         dimension_ * sizeof(float));
            }
        }
    }

    // Write ID mapping
    std::uint64_t map_size = id_to_cluster_.size();
    out.write(reinterpret_cast<const char*>(&map_size), sizeof(map_size));
    for (const auto& [id, cluster] : id_to_cluster_) {
        out.write(reinterpret_cast<const char*>(&id), sizeof(id));
        std::uint64_t cluster_u64 = cluster;
        out.write(reinterpret_cast<const char*>(&cluster_u64), sizeof(cluster_u64));
    }

    return out.good() ? ErrorCode::Ok : ErrorCode::IOError;
}

ErrorCode IVFIndex::deserialize(std::istream& in) {
    std::unique_lock lock(mutex_);

    // Read and validate header
    char magic[4];
    in.read(magic, 4);
    if (!in.good() || std::string(magic, 4) != "IVFX") {
        return ErrorCode::IOError;
    }

    std::uint32_t version;
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!in.good() || version != 1) {
        return ErrorCode::IOError;
    }

    std::uint64_t dim;
    in.read(reinterpret_cast<char*>(&dim), sizeof(dim));
    if (!in.good() || dim != dimension_) {
        return ErrorCode::DimensionMismatch;
    }

    std::uint32_t metric;
    in.read(reinterpret_cast<char*>(&metric), sizeof(metric));
    if (!in.good() || static_cast<DistanceMetric>(metric) != metric_) {
        return ErrorCode::InvalidParameter;
    }

    // Read centroids
    std::uint64_t num_clusters;
    in.read(reinterpret_cast<char*>(&num_clusters), sizeof(num_clusters));
    if (!in.good() || num_clusters == 0) {
        return ErrorCode::IOError;
    }

    std::vector<std::vector<float>> new_centroids;
    new_centroids.reserve(num_clusters);

    for (std::uint64_t i = 0; i < num_clusters; ++i) {
        std::vector<float> centroid(dimension_);
        in.read(reinterpret_cast<char*>(centroid.data()),
               dimension_ * sizeof(float));
        if (!in.good()) {
            return ErrorCode::IOError;
        }
        new_centroids.push_back(std::move(centroid));
    }

    // Read inverted lists
    std::vector<InvertedList> new_inverted_lists;
    new_inverted_lists.resize(num_clusters);

    for (std::uint64_t i = 0; i < num_clusters; ++i) {
        std::uint64_t list_size;
        in.read(reinterpret_cast<char*>(&list_size), sizeof(list_size));
        if (!in.good()) {
            return ErrorCode::IOError;
        }

        if (list_size > 0) {
            new_inverted_lists[i].ids.resize(list_size);
            in.read(reinterpret_cast<char*>(new_inverted_lists[i].ids.data()),
                   list_size * sizeof(std::uint64_t));
            if (!in.good()) {
                return ErrorCode::IOError;
            }

            new_inverted_lists[i].vectors.reserve(list_size);
            for (std::uint64_t j = 0; j < list_size; ++j) {
                std::vector<float> vec(dimension_);
                in.read(reinterpret_cast<char*>(vec.data()),
                       dimension_ * sizeof(float));
                if (!in.good()) {
                    return ErrorCode::IOError;
                }
                new_inverted_lists[i].vectors.push_back(std::move(vec));
            }
        }
    }

    // Read ID mapping
    std::uint64_t map_size;
    in.read(reinterpret_cast<char*>(&map_size), sizeof(map_size));
    if (!in.good()) {
        return ErrorCode::IOError;
    }

    std::unordered_map<std::uint64_t, std::size_t> new_id_to_cluster;
    new_id_to_cluster.reserve(map_size);

    for (std::uint64_t i = 0; i < map_size; ++i) {
        std::uint64_t id;
        std::uint64_t cluster;
        in.read(reinterpret_cast<char*>(&id), sizeof(id));
        in.read(reinterpret_cast<char*>(&cluster), sizeof(cluster));
        if (!in.good() || cluster >= num_clusters) {
            return ErrorCode::IOError;
        }
        new_id_to_cluster[id] = static_cast<std::size_t>(cluster);
    }

    // Validate integrity: check that mapping size matches total vectors
    std::size_t total_vectors = 0;
    for (const auto& inv_list : new_inverted_lists) {
        total_vectors += inv_list.size();
    }
    if (total_vectors != map_size) {
        return ErrorCode::InvalidState;
    }

    // All validation passed, update index state
    centroids_ = std::move(new_centroids);
    inverted_lists_ = std::move(new_inverted_lists);
    id_to_cluster_ = std::move(new_id_to_cluster);
    params_.n_clusters = num_clusters;

    return ErrorCode::Ok;
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

std::vector<std::size_t> IVFIndex::find_nearest_centroids(
    std::span<const float> vector,
    std::size_t n_probe) const {
    // Note: This method is called with mutex already held

    if (centroids_.empty()) {
        return {};
    }

    // Calculate distances to all centroids
    std::vector<std::pair<float, std::size_t>> centroid_distances;
    centroid_distances.reserve(centroids_.size());

    for (std::size_t i = 0; i < centroids_.size(); ++i) {
        float dist = calculate_distance(vector, centroids_[i]);
        centroid_distances.push_back({dist, i});
    }

    // Clamp n_probe to actual number of centroids
    n_probe = std::min(n_probe, centroids_.size());

    // Select n_probe nearest centroids using partial_sort
    std::partial_sort(
        centroid_distances.begin(),
        centroid_distances.begin() + n_probe,
        centroid_distances.end(),
        [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

    // Extract cluster IDs
    std::vector<std::size_t> result;
    result.reserve(n_probe);
    for (std::size_t i = 0; i < n_probe; ++i) {
        result.push_back(centroid_distances[i].second);
    }

    return result;
}

float IVFIndex::calculate_distance(std::span<const float> a, std::span<const float> b) const {
    return lynx::calculate_distance(a, b, metric_);
}

} // namespace lynx
