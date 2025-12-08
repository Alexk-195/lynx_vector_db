/**
 * @file lynx.cpp
 * @brief Lynx Vector Database implementation.
 *
 * Implementation of core functionality for the Lynx vector database.
 */

#include "lynx/lynx.h"
#include "vector_database_impl.h"
#include "vector_database_mps.h"
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

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<IVectorDatabase> IVectorDatabase::create(const Config& config) {
    // Use MPS-based implementation for thread-safety
    return std::make_shared<VectorDatabase_MPS>(config);
}

} // namespace lynx
