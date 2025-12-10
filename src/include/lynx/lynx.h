/**
 * @file lynx.h
 * @brief Lynx Vector Database - Public Interface
 *
 * This header defines the public API for the Lynx vector database.
 * All classes exposed here are pure virtual interfaces.
 *
 * @copyright MIT License
 */

#ifndef LYNX_LYNX_H
#define LYNX_LYNX_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace lynx {

// ============================================================================
// Forward Declarations
// ============================================================================

class IVectorDatabase;
class IVectorIndex;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Supported index types for vector storage and search.
 */
enum class IndexType {
    Flat,   ///< Brute-force search, O(N) - best for small datasets
    HNSW,   ///< Hierarchical Navigable Small World, O(log N) - best for speed/recall
    IVF,    ///< Inverted File Index - best for memory efficiency
};

/**
 * @brief Distance metrics for vector similarity calculation.
 */
enum class DistanceMetric {
    L2,         ///< Euclidean distance (L2 norm)
    Cosine,     ///< Cosine similarity (1 - cos(theta))
    DotProduct, ///< Negative dot product (-a·b)
};

/**
 * @brief Error codes for database operations.
 */
enum class ErrorCode {
    Ok = 0,              ///< Operation succeeded
    DimensionMismatch,   ///< Vector dimension doesn't match database dimension
    VectorNotFound,      ///< Requested vector ID not found
    IndexNotBuilt,       ///< Index not yet constructed
    InvalidParameter,    ///< Invalid parameter value
    InvalidState,        ///< Operation not valid in current state
    OutOfMemory,         ///< Memory allocation failed
    IOError,             ///< File I/O error
    NotImplemented,      ///< Feature not yet implemented
    Busy,                ///< Operation cannot be completed due to high load
};

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief A single search result item containing vector ID and distance.
 */
struct SearchResultItem {
    std::uint64_t id;    ///< Vector identifier
    float distance;      ///< Distance/similarity score (lower is more similar)
};

/**
 * @brief Container for search results with metadata.
 */
struct SearchResult {
    std::vector<SearchResultItem> items;  ///< Sorted results (nearest first)
    std::size_t total_candidates;         ///< Total candidates evaluated
    double query_time_ms;                 ///< Query execution time in milliseconds
};

/**
 * @brief Record for batch vector operations.
 */
struct VectorRecord {
    std::uint64_t id;              ///< Vector identifier
    std::vector<float> vector;     ///< Vector data
    std::optional<std::string> metadata;  ///< Optional metadata (JSON)
};

/**
 * @brief Database statistics and metrics.
 */
struct DatabaseStats {
    std::size_t vector_count;       ///< Number of vectors stored
    std::size_t dimension;          ///< Vector dimensionality
    std::size_t memory_usage_bytes; ///< Approximate memory usage
    std::size_t index_memory_bytes; ///< Index-specific memory usage
    double avg_query_time_ms;       ///< Average query time
    std::size_t total_queries;      ///< Total queries processed
    std::size_t total_inserts;      ///< Total inserts processed
};

/**
 * @brief Parameters for search operations.
 */
struct SearchParams {
    std::size_t ef_search = 50;     ///< HNSW: expansion factor during search
    std::size_t n_probe = 10;       ///< IVF: number of clusters to probe
    std::optional<std::function<bool(std::uint64_t)>> filter;  ///< Optional ID filter
};

/**
 * @brief HNSW-specific configuration parameters.
 */
struct HNSWParams {
    std::size_t m = 16;                  ///< Max connections per node per layer
    std::size_t ef_construction = 200;   ///< Expansion factor during construction
    std::size_t ef_search = 50;          ///< Default expansion factor during search
    std::size_t max_elements = 1000000;  ///< Maximum number of elements
    std::optional<std::uint64_t> random_seed = std::nullopt;  ///< Random seed (nullopt = non-deterministic)
};

/**
 * @brief IVF-specific configuration parameters.
 */
struct IVFParams {
    std::size_t n_clusters = 1024;  ///< Number of clusters (centroids)
    std::size_t n_probe = 10;       ///< Default clusters to probe during search
    bool use_pq = false;            ///< Enable Product Quantization
    std::size_t pq_subvectors = 8;  ///< Number of PQ subvectors (if use_pq)
};

/**
 * @brief Database configuration.
 */
struct Config {
    // Vector configuration
    std::size_t dimension = 128;                        ///< Vector dimensionality
    DistanceMetric distance_metric = DistanceMetric::L2; ///< Distance metric

    // Index configuration
    IndexType index_type = IndexType::HNSW;  ///< Index algorithm to use
    HNSWParams hnsw_params;                  ///< HNSW parameters (if applicable)
    IVFParams ivf_params;                    ///< IVF parameters (if applicable)

    // Threading configuration
    std::size_t num_query_threads = 0;   ///< Query worker threads (0 = auto)
    std::size_t num_index_threads = 2;   ///< Index worker threads

    // Storage configuration
    std::string data_path;      ///< Path for persistence (empty = in-memory)
    bool enable_wal = false;    ///< Enable write-ahead logging
    bool enable_mmap = false;   ///< Enable memory-mapped storage
};

// ============================================================================
// Interfaces
// ============================================================================

/**
 * @brief Abstract interface for vector index implementations.
 *
 * This interface defines the contract for all index types (HNSW, IVF, Flat).
 * Implementations are internal and not exposed in the public API.
 */
class IVectorIndex {
public:
    virtual ~IVectorIndex() = 0;

    // -------------------------------------------------------------------------
    // Vector Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Add a single vector to the index.
     * @param id Unique identifier for the vector
     * @param vector Vector data (must match index dimension)
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode add(std::uint64_t id, std::span<const float> vector) = 0;

    /**
     * @brief Remove a vector from the index.
     * @param id Vector identifier to remove
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode remove(std::uint64_t id) = 0;

    /**
     * @brief Check if a vector exists in the index.
     * @param id Vector identifier to check
     * @return true if vector exists, false otherwise
     */
    [[nodiscard]] virtual bool contains(std::uint64_t id) const = 0;

    // -------------------------------------------------------------------------
    // Search Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Search for k nearest neighbors.
     * @param query Query vector
     * @param k Number of neighbors to return
     * @param params Search parameters
     * @return Vector of (id, distance) pairs, sorted by distance
     */
    [[nodiscard]] virtual std::vector<SearchResultItem> search(
        std::span<const float> query,
        std::size_t k,
        const SearchParams& params) const = 0;

    // -------------------------------------------------------------------------
    // Batch Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Build index from a batch of vectors.
     * @param vectors Vector records to index
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode build(std::span<const VectorRecord> vectors) = 0;

    // -------------------------------------------------------------------------
    // Serialization
    // -------------------------------------------------------------------------

    /**
     * @brief Serialize index to output stream.
     * @param out Output stream
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode serialize(std::ostream& out) const = 0;

    /**
     * @brief Deserialize index from input stream.
     * @param in Input stream
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode deserialize(std::istream& in) = 0;

    // -------------------------------------------------------------------------
    // Properties
    // -------------------------------------------------------------------------

    /**
     * @brief Get the number of vectors in the index.
     * @return Vector count
     */
    [[nodiscard]] virtual std::size_t size() const = 0;

    /**
     * @brief Get the vector dimensionality.
     * @return Dimension
     */
    [[nodiscard]] virtual std::size_t dimension() const = 0;

    /**
     * @brief Get approximate memory usage in bytes.
     * @return Memory usage
     */
    [[nodiscard]] virtual std::size_t memory_usage() const = 0;
};

/**
 * @brief Abstract interface for the vector database.
 *
 * This is the primary interface for interacting with Lynx.
 * Obtain an instance via create_database() factory function.
 */
class IVectorDatabase {
public:
    virtual ~IVectorDatabase() = 0;

    // -------------------------------------------------------------------------
    // Single Vector Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Insert a single vector into the database.
     * @param record Vector record containing id, vector data, and optional metadata
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode insert(const VectorRecord& record) = 0;

    /**
     * @brief Remove a vector from the database.
     * @param id Vector identifier to remove
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode remove(std::uint64_t id) = 0;

    /**
     * @brief Check if a vector exists in the database.
     * @param id Vector identifier to check
     * @return true if vector exists, false otherwise
     */
    [[nodiscard]] virtual bool contains(std::uint64_t id) const = 0;

    /**
     * @brief Retrieve a vector record by its ID.
     * @param id Vector identifier
     * @return VectorRecord if found, empty optional otherwise
     */
    [[nodiscard]] virtual std::optional<VectorRecord> get(
        std::uint64_t id) const = 0;

    // -------------------------------------------------------------------------
    // Search Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Search for k nearest neighbors to a query vector.
     * @param query Query vector (must match configured dimension)
     * @param k Number of neighbors to return
     * @return SearchResult containing neighbors sorted by distance
     */
    [[nodiscard]] virtual SearchResult search(
        std::span<const float> query,
        std::size_t k) const = 0;

    /**
     * @brief Search with custom parameters.
     * @param query Query vector
     * @param k Number of neighbors to return
     * @param params Search parameters (ef_search, n_probe, filter)
     * @return SearchResult containing neighbors sorted by distance
     */
    [[nodiscard]] virtual SearchResult search(
        std::span<const float> query,
        std::size_t k,
        const SearchParams& params) const = 0;

    // -------------------------------------------------------------------------
    // Batch Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Insert multiple vectors in a batch.
     * @param records Vector records to insert
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode batch_insert(std::span<const VectorRecord> records) = 0;

    // -------------------------------------------------------------------------
    // Database Properties
    // -------------------------------------------------------------------------

    /**
     * @brief Get the number of vectors in the database.
     * @return Vector count
     */
    [[nodiscard]] virtual std::size_t size() const = 0;

    /**
     * @brief Get the configured vector dimensionality.
     * @return Dimension
     */
    [[nodiscard]] virtual std::size_t dimension() const = 0;

    /**
     * @brief Get database statistics and metrics.
     * @return DatabaseStats structure
     */
    [[nodiscard]] virtual DatabaseStats stats() const = 0;

    /**
     * @brief Get the current configuration.
     * @return Config structure
     */
    [[nodiscard]] virtual const Config& config() const = 0;

    // -------------------------------------------------------------------------
    // Persistence
    // -------------------------------------------------------------------------

    /**
     * @brief Flush all pending writes to storage.
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode flush() = 0;

    /**
     * @brief Save database to the configured data path.
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode save() = 0;

    /**
     * @brief Load database from the configured data path.
     * @return ErrorCode indicating success or failure
     */
    virtual ErrorCode load() = 0;

    /**
    * @brief Get library version string.
    * @return Version in format "major.minor.patch"
    */
    [[nodiscard]] static const char* version();

    /**
    * @brief Create a new vector database instance.
    * @param config Database configuration
    * @return Unique pointer to IVectorDatabase implementation
    */
    [[nodiscard]] static std::shared_ptr<IVectorDatabase> create(const Config& config);
};

// ============================================================================
// Utility Functions
// ============================================================================

 /**
 * @brief Get string representation of an error code.
 * @param code Error code
 * @return Human-readable error message
 */
 [[nodiscard]] const char* error_string(ErrorCode code);

/**
 * @brief Get string representation of an index type.
 * @param type Index type
 * @return Index type name
 */
[[nodiscard]] const char* index_type_string(IndexType type);

/**
 * @brief Get string representation of a distance metric.
 * @param metric Distance metric
 * @return Metric name
 */
[[nodiscard]] const char* distance_metric_string(DistanceMetric metric);

// ============================================================================
// Distance Metric Functions
// ============================================================================

/**
 * @brief Calculate L2 (Euclidean) distance between two vectors.
 *
 * Computes: sqrt(sum((a[i] - b[i])^2))
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @return L2 distance (always non-negative)
 */
[[nodiscard]] float distance_l2(std::span<const float> a, std::span<const float> b);

/**
 * @brief Calculate squared L2 distance between two vectors (faster than distance_l2).
 *
 * Computes: sum((a[i] - b[i])^2)
 * Useful when only relative distances matter (avoids sqrt).
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @return Squared L2 distance (always non-negative)
 */
[[nodiscard]] float distance_l2_squared(std::span<const float> a, std::span<const float> b);

/**
 * @brief Calculate Cosine distance between two vectors.
 *
 * Computes: 1 - (a·b) / (|a| * |b|)
 * Returns 0 for identical directions, 2 for opposite directions.
 * Commonly used for normalized vectors (e.g., text embeddings).
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @return Cosine distance in range [0, 2]
 */
[[nodiscard]] float distance_cosine(std::span<const float> a, std::span<const float> b);

/**
 * @brief Calculate negative dot product distance between two vectors.
 *
 * Computes: -(a·b)
 * Negative is used so that "smaller is more similar" (consistent with other metrics).
 * Optimal for pre-normalized vectors.
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @return Negative dot product
 */
[[nodiscard]] float distance_dot_product(std::span<const float> a, std::span<const float> b);

/**
 * @brief Calculate distance using the specified metric.
 *
 * Dispatches to the appropriate distance function based on metric type.
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @param metric Distance metric to use
 * @return Distance value (interpretation depends on metric)
 */
[[nodiscard]] float calculate_distance(
    std::span<const float> a,
    std::span<const float> b,
    DistanceMetric metric);

} // namespace lynx

#endif // LYNX_LYNX_H
