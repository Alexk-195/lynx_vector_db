/**
 * @file vector_database_ivf.cpp
 * @brief Implementation of VectorDatabase_IVF class
 */

#include "vector_database_ivf.h"
#include "record_iterator_impl.h"
#include "utils.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>

namespace lynx {

// ============================================================================
// Constructor
// ============================================================================

VectorDatabase_IVF::VectorDatabase_IVF(const Config& config)
    : config_(config),
      total_inserts_(0),
      total_queries_(0),
      total_query_time_ms_(0.0) {

    // Create IVF index using config's IVF parameters
    index_ = std::make_shared<IVFIndex>(
        config_.dimension,
        config_.distance_metric,
        config_.ivf_params
    );
}

// ============================================================================
// Single Vector Operations
// ============================================================================

ErrorCode VectorDatabase_IVF::insert(const VectorRecord& record) {
    // Validate dimension
    if (record.vector.size() != config_.dimension) {
        return ErrorCode::DimensionMismatch;
    }

    // Check if index has been built (has centroids)
    if (!index_->has_centroids()) {
        // Auto-initialize: build index with this first vector
        // Use a unique lock on vectors_mutex_ to ensure only one thread initializes
        std::unique_lock lock(vectors_mutex_);

        // Double-check after acquiring lock (another thread might have initialized)
        if (!index_->has_centroids()) {
            // Build the index with just this one vector
            std::vector<VectorRecord> init_records = {record};
            ErrorCode build_result = index_->build(init_records);
            if (build_result != ErrorCode::Ok) {
                return build_result;
            }

            // Store the vector
            vectors_[record.id] = record;
            total_inserts_.fetch_add(1);
            return ErrorCode::Ok;
        }
        // Another thread initialized, fall through to normal insert
        lock.unlock();
    }

    // Insert into vector storage
    {
        std::unique_lock lock(vectors_mutex_);
        vectors_[record.id] = record;
    }

    // Add to index (index has its own locking)
    ErrorCode result = index_->add(record.id, record.vector);

    if (result == ErrorCode::Ok) {
        total_inserts_.fetch_add(1);
    }

    return result;
}

ErrorCode VectorDatabase_IVF::remove(std::uint64_t id) {
    // Remove from index first (index has its own locking)
    ErrorCode result = index_->remove(id);

    // Remove from storage if index removal succeeded
    if (result == ErrorCode::Ok) {
        std::unique_lock lock(vectors_mutex_);
        auto it = vectors_.find(id);
        if (it != vectors_.end()) {
            vectors_.erase(it);
        }
    }

    return result;
}

bool VectorDatabase_IVF::contains(std::uint64_t id) const {
    std::shared_lock lock(vectors_mutex_);
    return vectors_.find(id) != vectors_.end();
}

std::optional<VectorRecord> VectorDatabase_IVF::get(std::uint64_t id) const {
    std::shared_lock lock(vectors_mutex_);
    auto it = vectors_.find(id);
    if (it == vectors_.end()) {
        return std::nullopt;
    }
    return it->second;
}

RecordRange VectorDatabase_IVF::all_records() const {
    // Create iterators using LockedIteratorImpl with shared_mutex
    using MapType = std::unordered_map<std::uint64_t, VectorRecord>;

    // Create a shared lock that will be held by all iterator copies
    auto lock = std::make_shared<std::shared_lock<std::shared_mutex>>(vectors_mutex_);

    auto begin_impl = std::make_shared<LockedIteratorImpl<MapType, std::shared_mutex>>(
        vectors_.begin(), lock);
    auto end_impl = std::make_shared<LockedIteratorImpl<MapType, std::shared_mutex>>(
        vectors_.end(), lock);

    return RecordRange(RecordIterator(begin_impl), RecordIterator(end_impl));
}

// ============================================================================
// Search Operations
// ============================================================================

SearchResult VectorDatabase_IVF::search(std::span<const float> query, std::size_t k) const {
    SearchParams default_params;
    default_params.n_probe = config_.ivf_params.n_probe;  // Use config's n_probe
    return search(query, k, default_params);
}

SearchResult VectorDatabase_IVF::search(std::span<const float> query, std::size_t k,
                                        const SearchParams& params) const {
    // Validate query dimension
    if (query.size() != config_.dimension) {
        return SearchResult{{}, 0, 0.0};
    }

    // Check if index has been built
    if (!index_->has_centroids()) {
        return SearchResult{{}, 0, 0.0};
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Search using IVF index
    auto results = index_->search(query, k, params);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    double query_time_ms = duration.count() / 1000.0;

    // Update statistics (thread-safe)
    total_queries_.fetch_add(1);
    {
        std::lock_guard lock(stats_mutex_);
        total_query_time_ms_ += query_time_ms;
    }

    SearchResult result;
    result.items = std::move(results);
    result.total_candidates = index_->size();  // IVF searched candidates
    result.query_time_ms = query_time_ms;
    return result;
}

// ============================================================================
// Batch Operations
// ============================================================================

ErrorCode VectorDatabase_IVF::batch_insert(std::span<const VectorRecord> records) {
    // Validate all records first
    for (const auto& record : records) {
        if (record.vector.size() != config_.dimension) {
            return ErrorCode::DimensionMismatch;
        }
    }

    // Build the index with all records (index has its own locking)
    ErrorCode build_result = index_->build(records);
    if (build_result != ErrorCode::Ok) {
        return build_result;
    }

    // Store all records in vector storage
    {
        std::unique_lock lock(vectors_mutex_);
        for (const auto& record : records) {
            vectors_[record.id] = record;
        }
    }

    total_inserts_.fetch_add(records.size());
    return ErrorCode::Ok;
}

// ============================================================================
// Database Properties
// ============================================================================

std::size_t VectorDatabase_IVF::size() const {
    std::shared_lock lock(vectors_mutex_);
    return vectors_.size();
}

std::size_t VectorDatabase_IVF::dimension() const {
    return config_.dimension;
}

DatabaseStats VectorDatabase_IVF::stats() const {
    DatabaseStats stats_result;

    // Get vector count with lock
    {
        std::shared_lock lock(vectors_mutex_);
        stats_result.vector_count = vectors_.size();

        // Calculate memory usage
        // Vectors in storage
        std::size_t per_vector_size = sizeof(std::uint64_t) +
                                      (config_.dimension * sizeof(float)) +
                                      sizeof(VectorRecord);
        stats_result.memory_usage_bytes = vectors_.size() * per_vector_size;
    }

    stats_result.dimension = config_.dimension;

    // Index memory (from IVFIndex)
    stats_result.index_memory_bytes = index_->memory_usage();

    // Get statistics atomically
    stats_result.total_queries = total_queries_.load();
    stats_result.total_inserts = total_inserts_.load();

    // Calculate average query time
    {
        std::lock_guard lock(stats_mutex_);
        if (stats_result.total_queries > 0) {
            stats_result.avg_query_time_ms = total_query_time_ms_ / stats_result.total_queries;
        } else {
            stats_result.avg_query_time_ms = 0.0;
        }
    }

    return stats_result;
}

const Config& VectorDatabase_IVF::config() const {
    return config_;
}

// ============================================================================
// Persistence
// ============================================================================

ErrorCode VectorDatabase_IVF::flush() {
    return ErrorCode::Ok;
}

ErrorCode VectorDatabase_IVF::save() {
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
        constexpr std::uint32_t kVersion = 2;  // Version 2 for IVF format

        out.write(reinterpret_cast<const char*>(&kMagicNumber), sizeof(kMagicNumber));
        out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));

        // Write index type marker
        std::uint8_t index_type_marker = static_cast<std::uint8_t>(IndexType::IVF);
        out.write(reinterpret_cast<const char*>(&index_type_marker), sizeof(index_type_marker));

        // Write configuration
        out.write(reinterpret_cast<const char*>(&config_.dimension), sizeof(config_.dimension));

        std::uint8_t metric_value = static_cast<std::uint8_t>(config_.distance_metric);
        out.write(reinterpret_cast<const char*>(&metric_value), sizeof(metric_value));

        // Write IVF parameters
        out.write(reinterpret_cast<const char*>(&config_.ivf_params.n_clusters),
                 sizeof(config_.ivf_params.n_clusters));
        out.write(reinterpret_cast<const char*>(&config_.ivf_params.n_probe),
                 sizeof(config_.ivf_params.n_probe));

        // Write statistics
        std::size_t inserts = total_inserts_.load();
        std::size_t queries = total_queries_.load();
        double query_time;
        {
            std::lock_guard lock(stats_mutex_);
            query_time = total_query_time_ms_;
        }
        out.write(reinterpret_cast<const char*>(&inserts), sizeof(inserts));
        out.write(reinterpret_cast<const char*>(&queries), sizeof(queries));
        out.write(reinterpret_cast<const char*>(&query_time), sizeof(query_time));

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

        // Serialize the IVF index
        ErrorCode index_result = index_->serialize(out);
        if (index_result != ErrorCode::Ok) {
            return index_result;
        }

        if (!out.good()) {
            return ErrorCode::IOError;
        }

        return ErrorCode::Ok;

    } catch (const std::exception&) {
        return ErrorCode::IOError;
    }
}

ErrorCode VectorDatabase_IVF::load() {
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
        if (version != 2) {
            return ErrorCode::IOError; // Unsupported version (expecting IVF format)
        }

        // Read index type marker
        std::uint8_t index_type_marker;
        in.read(reinterpret_cast<char*>(&index_type_marker), sizeof(index_type_marker));
        if (static_cast<IndexType>(index_type_marker) != IndexType::IVF) {
            return ErrorCode::IOError; // Wrong index type
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

        // Read IVF parameters
        std::size_t n_clusters, n_probe;
        in.read(reinterpret_cast<char*>(&n_clusters), sizeof(n_clusters));
        in.read(reinterpret_cast<char*>(&n_probe), sizeof(n_probe));

        // Validate IVF parameters match
        if (n_clusters != config_.ivf_params.n_clusters) {
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
                total_inserts_.store(0);
                total_queries_.store(0);
                {
                    std::lock_guard lock(stats_mutex_);
                    total_query_time_ms_ = 0.0;
                }
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

        // Deserialize the IVF index
        ErrorCode index_result = index_->deserialize(in);
        if (index_result != ErrorCode::Ok) {
            // Restore to empty state on error
            vectors_.clear();
            total_inserts_.store(0);
            total_queries_.store(0);
            {
                std::lock_guard lock(stats_mutex_);
                total_query_time_ms_ = 0.0;
            }
            return index_result;
        }

        if (!in.good()) {
            // Restore to empty state on error
            vectors_.clear();
            total_inserts_.store(0);
            total_queries_.store(0);
            {
                std::lock_guard lock(stats_mutex_);
                total_query_time_ms_ = 0.0;
            }
            return ErrorCode::IOError;
        }

        // Restore statistics
        total_inserts_.store(loaded_total_inserts);
        total_queries_.store(loaded_total_queries);
        {
            std::lock_guard lock(stats_mutex_);
            total_query_time_ms_ = loaded_total_query_time_ms;
        }

        return ErrorCode::Ok;

    } catch (const std::exception&) {
        // Restore to empty state on exception
        vectors_.clear();
        total_inserts_.store(0);
        total_queries_.store(0);
        {
            std::lock_guard lock(stats_mutex_);
            total_query_time_ms_ = 0.0;
        }
        return ErrorCode::IOError;
    }
}

} // namespace lynx
