/**
 * @file flat_index.cpp
 * @brief Implementation of FlatIndex class
 */

#include "flat_index.h"
#include "utils.h"
#include <algorithm>
#include <cstring>
#include <istream>
#include <ostream>

namespace lynx {

// ============================================================================
// Constructor
// ============================================================================

FlatIndex::FlatIndex(std::size_t dimension, DistanceMetric metric)
    : dimension_(dimension), metric_(metric) {}

// ============================================================================
// IVectorIndex Interface Implementation
// ============================================================================

ErrorCode FlatIndex::add(std::uint64_t id, std::span<const float> vector) {
    // Validate dimension
    if (vector.size() != dimension_) {
        return ErrorCode::DimensionMismatch;
    }

    // Add or update the vector
    vectors_[id] = std::vector<float>(vector.begin(), vector.end());
    return ErrorCode::Ok;
}

ErrorCode FlatIndex::remove(std::uint64_t id) {
    auto it = vectors_.find(id);
    if (it == vectors_.end()) {
        return ErrorCode::VectorNotFound;
    }
    vectors_.erase(it);
    return ErrorCode::Ok;
}

bool FlatIndex::contains(std::uint64_t id) const {
    return vectors_.find(id) != vectors_.end();
}

std::vector<SearchResultItem> FlatIndex::search(
    std::span<const float> query,
    std::size_t k,
    const SearchParams& params) const {

    // Validate query dimension
    if (query.size() != dimension_) {
        return {};  // Return empty results on dimension mismatch
    }

    // Brute-force search: calculate distance to all vectors
    std::vector<SearchResultItem> results;
    results.reserve(vectors_.size());

    for (const auto& [id, vector] : vectors_) {
        // Apply filter if provided
        if (params.filter && !(*params.filter)(id)) {
            continue;
        }

        float distance = calculate_distance(query, vector);
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

    return results;
}

ErrorCode FlatIndex::build(std::span<const VectorRecord> vectors) {
    // Clear existing data
    vectors_.clear();

    // Add all vectors
    for (const auto& record : vectors) {
        ErrorCode err = add(record.id, record.vector);
        if (err != ErrorCode::Ok) {
            // On error, clear partially built index and return
            vectors_.clear();
            return err;
        }
    }

    return ErrorCode::Ok;
}

ErrorCode FlatIndex::serialize(std::ostream& out) const {
    try {
        // Write magic number and version
        out.write(reinterpret_cast<const char*>(&kMagicNumber), sizeof(kMagicNumber));
        out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));

        // Write dimension
        out.write(reinterpret_cast<const char*>(&dimension_), sizeof(dimension_));

        // Write metric
        std::uint8_t metric_value = static_cast<std::uint8_t>(metric_);
        out.write(reinterpret_cast<const char*>(&metric_value), sizeof(metric_value));

        // Write number of vectors
        std::size_t num_vectors = vectors_.size();
        out.write(reinterpret_cast<const char*>(&num_vectors), sizeof(num_vectors));

        // Write each vector
        for (const auto& [id, vector] : vectors_) {
            // Write vector ID
            out.write(reinterpret_cast<const char*>(&id), sizeof(id));

            // Write vector data
            out.write(reinterpret_cast<const char*>(vector.data()),
                     vector.size() * sizeof(float));
        }

        if (!out.good()) {
            return ErrorCode::IOError;
        }

        return ErrorCode::Ok;

    } catch (const std::exception&) {
        return ErrorCode::IOError;
    }
}

ErrorCode FlatIndex::deserialize(std::istream& in) {
    try {
        // Read and verify magic number
        std::uint32_t magic_number;
        in.read(reinterpret_cast<char*>(&magic_number), sizeof(magic_number));
        if (magic_number != kMagicNumber) {
            return ErrorCode::IOError;  // Invalid file format
        }

        // Read and verify version
        std::uint32_t version;
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != kVersion) {
            return ErrorCode::IOError;  // Unsupported version
        }

        // Read dimension
        std::size_t dimension;
        in.read(reinterpret_cast<char*>(&dimension), sizeof(dimension));
        if (dimension != dimension_) {
            return ErrorCode::DimensionMismatch;
        }

        // Read metric
        std::uint8_t metric_value;
        in.read(reinterpret_cast<char*>(&metric_value), sizeof(metric_value));
        DistanceMetric loaded_metric = static_cast<DistanceMetric>(metric_value);
        if (loaded_metric != metric_) {
            return ErrorCode::InvalidParameter;
        }

        // Read number of vectors
        std::size_t num_vectors;
        in.read(reinterpret_cast<char*>(&num_vectors), sizeof(num_vectors));

        // Clear existing data
        vectors_.clear();

        // Read each vector
        for (std::size_t i = 0; i < num_vectors; ++i) {
            // Read vector ID
            std::uint64_t id;
            in.read(reinterpret_cast<char*>(&id), sizeof(id));

            // Read vector data
            std::vector<float> vector(dimension_);
            in.read(reinterpret_cast<char*>(vector.data()),
                   vector.size() * sizeof(float));

            // Insert into map
            vectors_[id] = std::move(vector);
        }

        if (!in.good()) {
            // Restore to empty state on error
            vectors_.clear();
            return ErrorCode::IOError;
        }

        return ErrorCode::Ok;

    } catch (const std::exception&) {
        // Restore to empty state on exception
        vectors_.clear();
        return ErrorCode::IOError;
    }
}

// ============================================================================
// Properties
// ============================================================================

std::size_t FlatIndex::size() const {
    return vectors_.size();
}

std::size_t FlatIndex::dimension() const {
    return dimension_;
}

std::size_t FlatIndex::memory_usage() const {
    // Calculate memory usage:
    // - Map overhead (estimated)
    // - Per-entry: key (uint64_t) + vector data (dimension * float)
    std::size_t overhead = sizeof(FlatIndex);
    std::size_t vector_storage = vectors_.size() * (sizeof(std::uint64_t) + dimension_ * sizeof(float));
    std::size_t map_overhead = vectors_.size() * 32;  // Estimated overhead per map entry

    return overhead + vector_storage + map_overhead;
}

// ============================================================================
// Helper Methods
// ============================================================================

float FlatIndex::calculate_distance(std::span<const float> a, std::span<const float> b) const {
    return utils::calculate_distance(a, b, metric_);
}

} // namespace lynx
