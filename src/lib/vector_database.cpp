/**
 * @file vector_database.cpp
 * @brief Unified vector database implementation
 *
 * @copyright MIT License
 */

#include "vector_database.h"
#include "flat_index.h"
#include "hnsw_index.h"
#include "ivf_index.h"
#include "utils.h"
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <algorithm>

namespace lynx {

// =============================================================================
// Constructor and Index Factory
// =============================================================================

VectorDatabase::VectorDatabase(const Config& config)
    : config_(config), index_(create_index()) {
    // Validate configuration
    if (config_.dimension == 0) {
        throw std::invalid_argument("Dimension must be greater than 0");
    }
}

std::shared_ptr<IVectorIndex> VectorDatabase::create_index() {
    switch (config_.index_type) {
        case IndexType::Flat:
            return std::make_shared<FlatIndex>(
                config_.dimension,
                config_.distance_metric
            );

        case IndexType::HNSW:
            return std::make_shared<HNSWIndex>(
                config_.dimension,
                config_.distance_metric,
                config_.hnsw_params
            );

        case IndexType::IVF:
            return std::make_shared<IVFIndex>(
                config_.dimension,
                config_.distance_metric,
                config_.ivf_params
            );

        default:
            throw std::invalid_argument("Unknown index type");
    }
}

// =============================================================================
// Single Vector Operations
// =============================================================================

ErrorCode VectorDatabase::insert(const VectorRecord& record) {
    // Validate dimension
    ErrorCode validation = validate_dimension(record.vector);
    if (validation != ErrorCode::Ok) {
        return validation;
    }

    // Check for duplicate ID
    if (vectors_.contains(record.id)) {
        return ErrorCode::InvalidParameter;
    }

    // Store vector in vectors_
    vectors_[record.id] = record;

    // Delegate to index
    ErrorCode result = index_->add(record.id, record.vector);
    if (result != ErrorCode::Ok) {
        // Rollback: remove from vectors_
        vectors_.erase(record.id);
        return result;
    }

    // Update statistics
    total_inserts_.fetch_add(1, std::memory_order_relaxed);

    return ErrorCode::Ok;
}

ErrorCode VectorDatabase::remove(std::uint64_t id) {
    // Check if exists
    auto it = vectors_.find(id);
    if (it == vectors_.end()) {
        return ErrorCode::VectorNotFound;
    }

    // Remove from index
    ErrorCode result = index_->remove(id);
    if (result != ErrorCode::Ok) {
        return result;
    }

    // Remove from vectors_
    vectors_.erase(it);

    return ErrorCode::Ok;
}

bool VectorDatabase::contains(std::uint64_t id) const {
    return vectors_.contains(id);
}

std::optional<VectorRecord> VectorDatabase::get(std::uint64_t id) const {
    auto it = vectors_.find(id);
    if (it == vectors_.end()) {
        return std::nullopt;
    }
    return it->second;
}

RecordRange VectorDatabase::all_records() const {
    // Create simple iterators (no locking for single-threaded version)
    auto begin_impl = std::make_shared<SimpleIteratorImpl<decltype(vectors_)>>(vectors_.begin());
    auto end_impl = std::make_shared<SimpleIteratorImpl<decltype(vectors_)>>(vectors_.end());

    return RecordRange(
        RecordIterator(begin_impl),
        RecordIterator(end_impl)
    );
}

// =============================================================================
// Search Operations
// =============================================================================

SearchResult VectorDatabase::search(std::span<const float> query, std::size_t k) const {
    SearchParams default_params;
    default_params.ef_search = config_.hnsw_params.ef_search;
    default_params.n_probe = config_.ivf_params.n_probe;
    return search(query, k, default_params);
}

SearchResult VectorDatabase::search(std::span<const float> query,
                                     std::size_t k,
                                     const SearchParams& params) const {
    // Validate dimension
    if (query.size() != config_.dimension) {
        return SearchResult{};  // Return empty result on error
    }

    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Delegate to index
    std::vector<SearchResultItem> items = index_->search(query, k, params);

    // Calculate timing
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // Update statistics (lock-free atomic operations)
    total_queries_.fetch_add(1, std::memory_order_relaxed);

    // For total_query_time_ms_, use compare-exchange
    double current = total_query_time_ms_.load(std::memory_order_relaxed);
    while (!total_query_time_ms_.compare_exchange_weak(current, current + elapsed_ms,
                                                         std::memory_order_relaxed)) {
        // Retry until successful
    }

    // Build result
    SearchResult result;
    result.items = std::move(items);
    result.total_candidates = items.size();
    result.query_time_ms = elapsed_ms;

    return result;
}

// =============================================================================
// Batch Operations
// =============================================================================

ErrorCode VectorDatabase::batch_insert(std::span<const VectorRecord> records) {
    // Validate all dimensions first
    for (const auto& record : records) {
        ErrorCode validation = validate_dimension(record.vector);
        if (validation != ErrorCode::Ok) {
            return validation;
        }
    }

    // HYBRID STRATEGY: Choose based on current state and index type

    // Strategy 1: Empty index → use bulk build (fastest)
    if (vectors_.empty()) {
        return bulk_build(records);
    }

    // Strategy 2: IVF with large batch → rebuild for better clustering
    if (config_.index_type == IndexType::IVF &&
        should_rebuild_ivf(records.size())) {
        return rebuild_with_merge(records);
    }

    // Strategy 3: Default → incremental insert (safest)
    return incremental_insert(records);
}

// =============================================================================
// Database Properties
// =============================================================================

std::size_t VectorDatabase::size() const {
    return vectors_.size();
}

std::size_t VectorDatabase::dimension() const {
    return config_.dimension;
}

DatabaseStats VectorDatabase::stats() const {
    DatabaseStats stats;
    stats.vector_count = vectors_.size();
    stats.dimension = config_.dimension;

    // Index memory
    stats.index_memory_bytes = index_->memory_usage();

    // Vector storage memory (approximate)
    std::size_t vector_memory = vectors_.size() * (
        sizeof(VectorRecord) +
        config_.dimension * sizeof(float)
    );
    stats.memory_usage_bytes = vector_memory + stats.index_memory_bytes;

    // Query statistics
    stats.total_queries = total_queries_.load(std::memory_order_relaxed);
    stats.total_inserts = total_inserts_.load(std::memory_order_relaxed);

    double total_time = total_query_time_ms_.load(std::memory_order_relaxed);
    stats.avg_query_time_ms = (stats.total_queries > 0)
        ? (total_time / stats.total_queries)
        : 0.0;

    return stats;
}

// =============================================================================
// Persistence
// =============================================================================

ErrorCode VectorDatabase::flush() {
    // For in-memory databases, flush is same as save
    return save();
}

ErrorCode VectorDatabase::save() {
    if (config_.data_path.empty()) {
        return ErrorCode::InvalidParameter;
    }

    try {
        // Create directory if it doesn't exist
        std::filesystem::create_directories(config_.data_path);

        // 1. Save index
        std::string index_path = config_.data_path + "/index.bin";
        std::ofstream index_file(index_path, std::ios::binary);
        if (!index_file) {
            return ErrorCode::IOError;
        }

        ErrorCode result = index_->serialize(index_file);
        if (result != ErrorCode::Ok) {
            return result;
        }
        index_file.close();

        // 2. Save vectors (with metadata)
        std::string vectors_path = config_.data_path + "/vectors.bin";
        std::ofstream vectors_file(vectors_path, std::ios::binary);
        if (!vectors_file) {
            return ErrorCode::IOError;
        }

        // Write header
        std::uint32_t magic = kMagicNumber;
        std::uint32_t version = kVersion;
        std::uint64_t count = vectors_.size();

        vectors_file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        vectors_file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        vectors_file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        // Write dimension
        std::uint64_t dim = config_.dimension;
        vectors_file.write(reinterpret_cast<const char*>(&dim), sizeof(dim));

        // Write vectors with metadata
        for (const auto& [id, record] : vectors_) {
            // Write ID
            vectors_file.write(reinterpret_cast<const char*>(&id), sizeof(id));

            // Write vector data
            vectors_file.write(
                reinterpret_cast<const char*>(record.vector.data()),
                record.vector.size() * sizeof(float)
            );

            // Write metadata length and data
            std::uint32_t meta_len = record.metadata.has_value()
                ? static_cast<std::uint32_t>(record.metadata->size()) : 0;
            vectors_file.write(reinterpret_cast<const char*>(&meta_len), sizeof(meta_len));
            if (meta_len > 0) {
                vectors_file.write(record.metadata->data(), meta_len);
            }
        }

        vectors_file.close();

        return ErrorCode::Ok;

    } catch (const std::exception&) {
        return ErrorCode::IOError;
    }
}

ErrorCode VectorDatabase::load() {
    if (config_.data_path.empty()) {
        return ErrorCode::InvalidParameter;
    }

    try {
        // 1. Load index
        std::string index_path = config_.data_path + "/index.bin";
        std::ifstream index_file(index_path, std::ios::binary);
        if (!index_file) {
            return ErrorCode::IOError;
        }

        ErrorCode result = index_->deserialize(index_file);
        if (result != ErrorCode::Ok) {
            return result;
        }
        index_file.close();

        // 2. Load vectors
        std::string vectors_path = config_.data_path + "/vectors.bin";
        std::ifstream vectors_file(vectors_path, std::ios::binary);
        if (!vectors_file) {
            return ErrorCode::IOError;
        }

        // Read header
        std::uint32_t magic, version;
        std::uint64_t count;
        vectors_file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        vectors_file.read(reinterpret_cast<char*>(&version), sizeof(version));
        vectors_file.read(reinterpret_cast<char*>(&count), sizeof(count));

        if (magic != kMagicNumber) {
            return ErrorCode::IOError;
        }

        // Read dimension
        std::uint64_t dim;
        vectors_file.read(reinterpret_cast<char*>(&dim), sizeof(dim));
        if (dim != config_.dimension) {
            return ErrorCode::DimensionMismatch;
        }

        // Read vectors
        vectors_.clear();
        for (std::uint64_t i = 0; i < count; ++i) {
            // Read ID
            std::uint64_t id;
            vectors_file.read(reinterpret_cast<char*>(&id), sizeof(id));

            // Read vector data
            std::vector<float> vector(config_.dimension);
            vectors_file.read(
                reinterpret_cast<char*>(vector.data()),
                config_.dimension * sizeof(float)
            );

            // Read metadata
            std::uint32_t meta_len;
            vectors_file.read(reinterpret_cast<char*>(&meta_len), sizeof(meta_len));
            std::optional<std::string> metadata;
            if (meta_len > 0) {
                std::string meta_str(meta_len, '\0');
                vectors_file.read(meta_str.data(), meta_len);
                metadata = meta_str;
            }

            // Store record
            VectorRecord record{id, std::move(vector), metadata};
            vectors_[id] = std::move(record);
        }

        vectors_file.close();

        // Update statistics
        total_inserts_.store(count, std::memory_order_relaxed);

        return ErrorCode::Ok;

    } catch (const std::exception&) {
        return ErrorCode::IOError;
    }
}

// =============================================================================
// Helper Methods
// =============================================================================

ErrorCode VectorDatabase::validate_dimension(std::span<const float> vector) const {
    if (vector.size() != config_.dimension) {
        return ErrorCode::DimensionMismatch;
    }
    return ErrorCode::Ok;
}

double VectorDatabase::get_time_ms() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double, std::milli>(duration).count();
}

bool VectorDatabase::should_rebuild_ivf(std::size_t batch_size) const {
    // Rebuild if batch adds >50% more data
    // Rationale: k-means clustering with all data produces better centroids
    return batch_size > vectors_.size() * 0.5;
}

ErrorCode VectorDatabase::bulk_build(std::span<const VectorRecord> records) {
    ErrorCode result = index_->build(records);
    if (result == ErrorCode::Ok) {
        for (const auto& record : records) {
            vectors_[record.id] = record;
        }
        total_inserts_.fetch_add(records.size(), std::memory_order_relaxed);
    }
    return result;
}

ErrorCode VectorDatabase::rebuild_with_merge(std::span<const VectorRecord> records) {
    // Merge existing + new vectors
    std::vector<VectorRecord> all_records;
    all_records.reserve(vectors_.size() + records.size());

    // Add existing vectors
    for (const auto& [id, record] : vectors_) {
        all_records.push_back(record);
    }

    // Add new vectors
    all_records.insert(all_records.end(), records.begin(), records.end());

    // Rebuild index with all data
    ErrorCode result = index_->build(all_records);
    if (result == ErrorCode::Ok) {
        // Update vector storage
        for (const auto& record : records) {
            vectors_[record.id] = record;
        }
        total_inserts_.fetch_add(records.size(), std::memory_order_relaxed);
    }
    return result;
}

ErrorCode VectorDatabase::incremental_insert(std::span<const VectorRecord> records) {
    for (const auto& record : records) {
        // Check for duplicate ID
        if (vectors_.contains(record.id)) {
            return ErrorCode::InvalidParameter;
        }

        // Store vector
        vectors_[record.id] = record;

        // Add to index
        ErrorCode result = index_->add(record.id, record.vector);
        if (result != ErrorCode::Ok) {
            // Rollback this insert
            vectors_.erase(record.id);
            return result;
        }

        total_inserts_.fetch_add(1, std::memory_order_relaxed);
    }
    return ErrorCode::Ok;
}

} // namespace lynx
