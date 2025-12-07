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
    explicit VectorDatabase_Impl(const Config& config) : config_(config) {}

    // -------------------------------------------------------------------------
    // Single Vector Operations
    // -------------------------------------------------------------------------

    ErrorCode insert(const VectorRecord& record) override {
        (void)record;
        return ErrorCode::NotImplemented;
    }

    ErrorCode remove(std::uint64_t id) override {
        (void)id;
        return ErrorCode::NotImplemented;
    }

    bool contains(std::uint64_t id) const override {
        (void)id;
        return false;
    }

    std::optional<VectorRecord> get(std::uint64_t id) const override {
        (void)id;
        return std::nullopt;
    }

    // -------------------------------------------------------------------------
    // Search Operations
    // -------------------------------------------------------------------------

    SearchResult search(std::span<const float> query, std::size_t k) const override {
        (void)query;
        (void)k;
        return SearchResult{{}, 0, 0.0};
    }

    SearchResult search(std::span<const float> query, std::size_t k,
                       const SearchParams& params) const override {
        (void)query;
        (void)k;
        (void)params;
        return SearchResult{{}, 0, 0.0};
    }

    // -------------------------------------------------------------------------
    // Batch Operations
    // -------------------------------------------------------------------------

    ErrorCode batch_insert(std::span<const VectorRecord> records) override {
        (void)records;
        return ErrorCode::NotImplemented;
    }

    // -------------------------------------------------------------------------
    // Database Properties
    // -------------------------------------------------------------------------

    std::size_t size() const override {
        return 0;
    }

    std::size_t dimension() const override {
        return config_.dimension;
    }

    DatabaseStats stats() const override {
        DatabaseStats stats;
        stats.vector_count = 0;
        stats.dimension = config_.dimension;
        stats.memory_usage_bytes = 0;
        stats.index_memory_bytes = 0;
        stats.avg_query_time_ms = 0.0;
        stats.total_queries = 0;
        stats.total_inserts = 0;
        return stats;
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
};


// ============================================================================
// Factory Function Placeholders
// ============================================================================

std::shared_ptr<IVectorDatabase> IVectorDatabase::create(const Config& config) {
    return std::make_shared<VectorDatabase_Impl>(config);
}

} // namespace lynx
