/**
 * @file vector_database_impl.cpp
 * @brief Implementation of VectorDatabase_Impl class
 */

#include "vector_database_impl.h"
#include <algorithm>
#include <chrono>
#include <fstream>

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
    return ErrorCode::Ok;
}

ErrorCode VectorDatabase_Impl::save() {
    // Check if data_path is specified
    if (config_.data_path.empty()) {
        return ErrorCode::InvalidParameter;
    }

    // Open file for writing
    std::ofstream out(config_.data_path, std::ios::binary);
    if (!out) {
        return ErrorCode::IOError;
    }

    try {
        // Write magic number and version for file format verification
        constexpr std::uint32_t kMagicNumber = 0x4C594E58; // "LYNX" in hex
        constexpr std::uint32_t kVersion = 1;

        out.write(reinterpret_cast<const char*>(&kMagicNumber), sizeof(kMagicNumber));
        out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));

        // Write configuration
        out.write(reinterpret_cast<const char*>(&config_.dimension), sizeof(config_.dimension));

        std::uint8_t metric_value = static_cast<std::uint8_t>(config_.distance_metric);
        out.write(reinterpret_cast<const char*>(&metric_value), sizeof(metric_value));

        // Write statistics
        out.write(reinterpret_cast<const char*>(&total_inserts_), sizeof(total_inserts_));
        out.write(reinterpret_cast<const char*>(&total_queries_), sizeof(total_queries_));
        out.write(reinterpret_cast<const char*>(&total_query_time_ms_), sizeof(total_query_time_ms_));

        // Write number of vectors
        std::size_t num_vectors = vectors_.size();
        out.write(reinterpret_cast<const char*>(&num_vectors), sizeof(num_vectors));

        // Write each vector
        for (const auto& [id, record] : vectors_) {
            // Write vector ID
            out.write(reinterpret_cast<const char*>(&id), sizeof(id));

            // Write vector size (for validation)
            std::size_t vector_size = record.vector.size();
            out.write(reinterpret_cast<const char*>(&vector_size), sizeof(vector_size));

            // Write vector data
            out.write(reinterpret_cast<const char*>(record.vector.data()),
                     record.vector.size() * sizeof(float));

            // Write metadata (if present)
            bool has_metadata = record.metadata.has_value();
            out.write(reinterpret_cast<const char*>(&has_metadata), sizeof(has_metadata));
            if (has_metadata) {
                std::size_t metadata_size = record.metadata->size();
                out.write(reinterpret_cast<const char*>(&metadata_size), sizeof(metadata_size));
                out.write(record.metadata->data(), metadata_size);
            }
        }

        if (!out.good()) {
            return ErrorCode::IOError;
        }

        return ErrorCode::Ok;

    } catch (const std::exception&) {
        return ErrorCode::IOError;
    }
}

ErrorCode VectorDatabase_Impl::load() {
    // Check if data_path is specified
    if (config_.data_path.empty()) {
        return ErrorCode::InvalidParameter;
    }

    // Open file for reading
    std::ifstream in(config_.data_path, std::ios::binary);
    if (!in) {
        return ErrorCode::IOError;
    }

    try {
        // Read and verify magic number
        constexpr std::uint32_t kExpectedMagic = 0x4C594E58; // "LYNX"
        std::uint32_t magic_number;
        in.read(reinterpret_cast<char*>(&magic_number), sizeof(magic_number));
        if (magic_number != kExpectedMagic) {
            return ErrorCode::IOError; // Invalid file format
        }

        // Read and verify version
        std::uint32_t version;
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != 1) {
            return ErrorCode::IOError; // Unsupported version
        }

        // Read configuration
        std::size_t dimension;
        in.read(reinterpret_cast<char*>(&dimension), sizeof(dimension));
        if (dimension != config_.dimension) {
            return ErrorCode::DimensionMismatch;
        }

        std::uint8_t metric_value;
        in.read(reinterpret_cast<char*>(&metric_value), sizeof(metric_value));
        DistanceMetric loaded_metric = static_cast<DistanceMetric>(metric_value);
        if (loaded_metric != config_.distance_metric) {
            return ErrorCode::InvalidParameter;
        }

        // Read statistics
        std::size_t loaded_total_inserts;
        std::size_t loaded_total_queries;
        double loaded_total_query_time_ms;
        in.read(reinterpret_cast<char*>(&loaded_total_inserts), sizeof(loaded_total_inserts));
        in.read(reinterpret_cast<char*>(&loaded_total_queries), sizeof(loaded_total_queries));
        in.read(reinterpret_cast<char*>(&loaded_total_query_time_ms), sizeof(loaded_total_query_time_ms));

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

            // Read vector size
            std::size_t vector_size;
            in.read(reinterpret_cast<char*>(&vector_size), sizeof(vector_size));

            // Validate vector size
            if (vector_size != config_.dimension) {
                // Restore to empty state on error
                vectors_.clear();
                total_inserts_ = 0;
                total_queries_ = 0;
                total_query_time_ms_ = 0.0;
                return ErrorCode::DimensionMismatch;
            }

            // Read vector data
            std::vector<float> vector(vector_size);
            in.read(reinterpret_cast<char*>(vector.data()),
                   vector.size() * sizeof(float));

            // Read metadata (if present)
            bool has_metadata;
            in.read(reinterpret_cast<char*>(&has_metadata), sizeof(has_metadata));
            std::optional<std::string> metadata;
            if (has_metadata) {
                std::size_t metadata_size;
                in.read(reinterpret_cast<char*>(&metadata_size), sizeof(metadata_size));
                std::string metadata_str(metadata_size, '\0');
                in.read(metadata_str.data(), metadata_size);
                metadata = std::move(metadata_str);
            }

            // Create VectorRecord and insert into map
            VectorRecord record{id, std::move(vector), std::move(metadata)};
            vectors_[id] = std::move(record);
        }

        if (!in.good()) {
            // Restore to empty state on error
            vectors_.clear();
            total_inserts_ = 0;
            total_queries_ = 0;
            total_query_time_ms_ = 0.0;
            return ErrorCode::IOError;
        }

        // Restore statistics
        total_inserts_ = loaded_total_inserts;
        total_queries_ = loaded_total_queries;
        total_query_time_ms_ = loaded_total_query_time_ms;

        return ErrorCode::Ok;

    } catch (const std::exception&) {
        // Restore to empty state on exception
        vectors_.clear();
        total_inserts_ = 0;
        total_queries_ = 0;
        total_query_time_ms_ = 0.0;
        return ErrorCode::IOError;
    }
}

} // namespace lynx
