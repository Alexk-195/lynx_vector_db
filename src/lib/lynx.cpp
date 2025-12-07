/**
 * @file placeholder.cpp
 * @brief Placeholder implementation for initial build.
 *
 * This file provides minimal implementations to allow the project to compile.
 * Real implementations will be added in subsequent phases.
 */

#include "lynx/lynx.h"
#include <stdexcept>

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


class VectorDatabase_Impl : public IVectorDatabase  {
public:
    explicit VectorDatabase_Impl(const Config& config) {
        (void)config;
    }
    // TODO: implement stubs for all pure virtual functions
};


// ============================================================================
// Factory Function Placeholders
// ============================================================================

std::shared_ptr<IVectorDatabase> IVectorDatabase::create(const Config& config) {
    return std::make_shared<VectorDatabase_Impl>(config);
}

} // namespace lynx
