/**
 * @file lynx.cpp
 * @brief Lynx Vector Database implementation.
 *
 * Implementation of core functionality for the Lynx vector database.
 */

#include "lynx/lynx.h"
#include "lynx/utils.h"
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
// These public API functions now delegate to the centralized implementations
// in utils.h/utils.cpp to avoid code duplication.

float distance_l2_squared(std::span<const float> a, std::span<const float> b) {
    return utils::calculate_l2_squared(a, b);
}

float distance_l2(std::span<const float> a, std::span<const float> b) {
    return utils::calculate_l2(a, b);
}

float distance_cosine(std::span<const float> a, std::span<const float> b) {
    return utils::calculate_cosine(a, b);
}

float distance_dot_product(std::span<const float> a, std::span<const float> b) {
    return utils::calculate_dot_product(a, b);
}

float calculate_distance(
    std::span<const float> a,
    std::span<const float> b,
    DistanceMetric metric) {
    return utils::calculate_distance(a, b, metric);
}

// ============================================================================
// Factory Functions
// ============================================================================

std::shared_ptr<IVectorDatabase> IVectorDatabase::create(const Config& config) {
    // Use MPS-based implementation for thread-safety
    return std::make_shared<VectorDatabase_MPS>(config);
}

} // namespace lynx
