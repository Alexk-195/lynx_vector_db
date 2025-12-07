/**
 * @file lynx.cpp
 * @brief Lynx Vector Database implementation.
 *
 * Implementation of core functionality for the Lynx vector database.
 */

#include "lynx/lynx.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <chrono>

namespace lynx {

// ============================================================================
// Utility Function Implementations
// ============================================================================

const char* error_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::Ok:               return "Ok";
        case ErrorCode::DimensionMismatch: return "Dimension mismatch";
        case ErrorCode::VectorNotFound:   return "Vector not found";
        case ErrorCode::IndexNotBuilt:    return "Index not built";
        case ErrorCode::InvalidParameter: return "Invalid parameter";
        case ErrorCode::InvalidState:     return "Invalid state";
        case ErrorCode::OutOfMemory:      return "Out of memory";
        case ErrorCode::IOError:          return "I/O error";
        case ErrorCode::NotImplemented:   return "Not implemented";
        default:                          return "Unknown error";
    }
}

const char* index_type_string(IndexType type) {
    switch (type) {
        case IndexType::Flat: return "Flat";
        case IndexType::HNSW: return "HNSW";
        case IndexType::IVF:  return "IVF";
        default:              return "Unknown";
    }
}

const char* distance_metric_string(DistanceMetric metric) {
    switch (metric) {
        case DistanceMetric::L2:         return "L2 (Euclidean)";
        case DistanceMetric::Cosine:     return "Cosine";
        case DistanceMetric::DotProduct: return "Dot Product";
        default:                         return "Unknown";
    }
}

const char* IVectorDatabase::version() {
    return "0.1.0";
}

// ============================================================================
// Distance Metric Implementations
// ============================================================================

float distance_l2_squared(std::span<const float> a, std::span<const float> b) {
    // Verify dimensions match
    if (a.size() != b.size()) {
        return -1.0f; // Error indicator
    }

    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

float distance_l2(std::span<const float> a, std::span<const float> b) {
    const float squared = distance_l2_squared(a, b);
    if (squared < 0.0f) {
        return -1.0f; // Error indicator (dimension mismatch)
    }
    return std::sqrt(squared);
}

float distance_cosine(std::span<const float> a, std::span<const float> b) {
    // Verify dimensions match
    if (a.size() != b.size()) {
        return -1.0f; // Error indicator
    }

    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (std::size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);

    // Handle zero vectors (avoid division by zero)
    if (norm_a < 1e-10f || norm_b < 1e-10f) {
        return 1.0f; // Maximum dissimilarity for zero vectors
    }

    // Cosine similarity: dot(a,b) / (|a| * |b|)
    const float cosine_similarity = dot_product / (norm_a * norm_b);

    // Clamp to [-1, 1] to handle floating point errors
    const float clamped = std::clamp(cosine_similarity, -1.0f, 1.0f);

    // Return cosine distance: 1 - cosine_similarity
    // Range is [0, 2]: 0 for identical, 1 for orthogonal, 2 for opposite
    return 1.0f - clamped;
}

float distance_dot_product(std::span<const float> a, std::span<const float> b) {
    // Verify dimensions match
    if (a.size() != b.size()) {
        return -1.0f; // Error indicator
    }

    float dot_product = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
    }

    // Return negative dot product (so smaller means more similar)
    return -dot_product;
}

float calculate_distance(
    std::span<const float> a,
    std::span<const float> b,
    DistanceMetric metric) {

    switch (metric) {
        case DistanceMetric::L2:
            return distance_l2(a, b);
        case DistanceMetric::Cosine:
            return distance_cosine(a, b);
        case DistanceMetric::DotProduct:
            return distance_dot_product(a, b);
        default:
            return -1.0f; // Error indicator for unknown metric
    }
}


class VectorDatabase_Impl : public IVectorDatabase  {
public:
    explicit VectorDatabase_Impl(const Config& config)
        : config_(config),
          total_inserts_(0),
          total_queries_(0),
          total_query_time_ms_(0.0) {}

    // -------------------------------------------------------------------------
    // Single Vector Operations
    // -------------------------------------------------------------------------

    ErrorCode insert(const VectorRecord& record) override {
        // Validate dimension
        if (record.vector.size() != config_.dimension) {
            return ErrorCode::DimensionMismatch;
        }

        // Insert or update the vector
        vectors_[record.id] = record;
        total_inserts_++;
        return ErrorCode::Ok;
    }

    ErrorCode remove(std::uint64_t id) override {
        auto it = vectors_.find(id);
        if (it == vectors_.end()) {
            return ErrorCode::VectorNotFound;
        }
        vectors_.erase(it);
        return ErrorCode::Ok;
    }

    bool contains(std::uint64_t id) const override {
        return vectors_.find(id) != vectors_.end();
    }

    std::optional<VectorRecord> get(std::uint64_t id) const override {
        auto it = vectors_.find(id);
        if (it == vectors_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    // -------------------------------------------------------------------------
    // Search Operations
    // -------------------------------------------------------------------------

    SearchResult search(std::span<const float> query, std::size_t k) const override {
        SearchParams default_params;
        return search(query, k, default_params);
    }

    SearchResult search(std::span<const float> query, std::size_t k,
                       const SearchParams& params) const override {
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

    // -------------------------------------------------------------------------
    // Batch Operations
    // -------------------------------------------------------------------------

    ErrorCode batch_insert(std::span<const VectorRecord> records) override {
        for (const auto& record : records) {
            ErrorCode err = insert(record);
            if (err != ErrorCode::Ok) {
                return err;
            }
        }
        return ErrorCode::Ok;
    }

    // -------------------------------------------------------------------------
    // Database Properties
    // -------------------------------------------------------------------------

    std::size_t size() const override {
        return vectors_.size();
    }

    std::size_t dimension() const override {
        return config_.dimension;
    }

    DatabaseStats stats() const override {
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

    const Config& config() const override {
        return config_;
    }

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    ErrorCode flush() override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode save() override {
        return ErrorCode::NotImplemented;
    }

    ErrorCode load() override {
        return ErrorCode::NotImplemented;
    }

private:
    Config config_;
    std::unordered_map<std::uint64_t, VectorRecord> vectors_;
    std::size_t total_inserts_;
    std::size_t total_queries_;
    double total_query_time_ms_;
};


// ============================================================================
// Factory Function Placeholders
// ============================================================================

std::shared_ptr<IVectorDatabase> IVectorDatabase::create(const Config& config) {
    return std::make_shared<VectorDatabase_Impl>(config);
}

} // namespace lynx
