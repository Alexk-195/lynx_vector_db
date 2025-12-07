/**
 * @file vector_database_impl.cpp
 * @brief Implementation of VectorDatabase_Impl class
 */

#include "vector_database_impl.h"
#include <algorithm>
#include <chrono>

namespace lynx {

// ============================================================================
// Constructor
// ============================================================================

VectorDatabase_Impl::VectorDatabase_Impl(const Config& config)
    : config_(config),
      total_inserts_(0),
      total_queries_(0),
      total_query_time_ms_(0.0) {}

// ============================================================================
// Single Vector Operations
// ============================================================================

ErrorCode VectorDatabase_Impl::insert(const VectorRecord& record) {
    // Validate dimension
    if (record.vector.size() != config_.dimension) {
        return ErrorCode::DimensionMismatch;
    }

    // Insert or update the vector
    vectors_[record.id] = record;
    total_inserts_++;
    return ErrorCode::Ok;
}

ErrorCode VectorDatabase_Impl::remove(std::uint64_t id) {
    auto it = vectors_.find(id);
    if (it == vectors_.end()) {
        return ErrorCode::VectorNotFound;
    }
    vectors_.erase(it);
    return ErrorCode::Ok;
}

bool VectorDatabase_Impl::contains(std::uint64_t id) const {
    return vectors_.find(id) != vectors_.end();
}

std::optional<VectorRecord> VectorDatabase_Impl::get(std::uint64_t id) const {
    auto it = vectors_.find(id);
    if (it == vectors_.end()) {
        return std::nullopt;
    }
    return it->second;
}

// ============================================================================
// Search Operations
// ============================================================================

SearchResult VectorDatabase_Impl::search(std::span<const float> query, std::size_t k) const {
    SearchParams default_params;
    return search(query, k, default_params);
}

SearchResult VectorDatabase_Impl::search(std::span<const float> query, std::size_t k,
                                        const SearchParams& params) const {
    // Validate query dimension
    if (query.size() != config_.dimension) {
        return SearchResult{{}, 0, 0.0};
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Brute-force search: calculate distance to all vectors
    std::vector<SearchResultItem> results;
    results.reserve(vectors_.size());

    for (const auto& [id, record] : vectors_) {
        // Apply filter if provided
        if (params.filter && !(*params.filter)(id)) {
            continue;
        }

        float distance = calculate_distance(query, record.vector, config_.distance_metric);
        results.push_back({id, distance});
    }

    // Sort by distance (ascending)
    std::sort(results.begin(), results.end(),
             [](const SearchResultItem& a, const SearchResultItem& b) {
                 return a.distance < b.distance;
             });

    // Keep only top-k results
    if (results.size() > k) {
        results.resize(k);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double query_time_ms = duration.count() / 1000.0;

    // Update statistics (mutable for const method)
    const_cast<VectorDatabase_Impl*>(this)->total_queries_++;
    const_cast<VectorDatabase_Impl*>(this)->total_query_time_ms_ += query_time_ms;

    SearchResult result;
    result.items = std::move(results);
    result.total_candidates = vectors_.size();
    result.query_time_ms = query_time_ms;
    return result;
}

// ============================================================================
// Batch Operations
// ============================================================================

ErrorCode VectorDatabase_Impl::batch_insert(std::span<const VectorRecord> records) {
    for (const auto& record : records) {
        ErrorCode err = insert(record);
        if (err != ErrorCode::Ok) {
            return err;
        }
    }
    return ErrorCode::Ok;
}

// ============================================================================
// Database Properties
// ============================================================================

std::size_t VectorDatabase_Impl::size() const {
    return vectors_.size();
}

std::size_t VectorDatabase_Impl::dimension() const {
    return config_.dimension;
}

DatabaseStats VectorDatabase_Impl::stats() const {
    DatabaseStats stats_result;
    stats_result.vector_count = vectors_.size();
    stats_result.dimension = config_.dimension;

    // Calculate memory usage
    // Each vector: sizeof(uint64_t) + dimension * sizeof(float) + overhead
    std::size_t per_vector_size = sizeof(std::uint64_t) +
                                  (config_.dimension * sizeof(float)) +
                                  sizeof(VectorRecord);
    stats_result.memory_usage_bytes = vectors_.size() * per_vector_size;
    stats_result.index_memory_bytes = 0; // No index yet (brute force)

    // Calculate average query time
    if (total_queries_ > 0) {
        stats_result.avg_query_time_ms = total_query_time_ms_ / total_queries_;
    } else {
        stats_result.avg_query_time_ms = 0.0;
    }

    stats_result.total_queries = total_queries_;
    stats_result.total_inserts = total_inserts_;

    return stats_result;
}

const Config& VectorDatabase_Impl::config() const {
    return config_;
}

// ============================================================================
// Persistence
// ============================================================================

ErrorCode VectorDatabase_Impl::flush() {
    return ErrorCode::NotImplemented;
}

ErrorCode VectorDatabase_Impl::save() {
    return ErrorCode::NotImplemented;
}

ErrorCode VectorDatabase_Impl::load() {
    return ErrorCode::NotImplemented;
}

} // namespace lynx
