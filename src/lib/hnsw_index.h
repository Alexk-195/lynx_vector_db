/**
 * @file hnsw_index.h
 * @brief HNSW (Hierarchical Navigable Small World) Index Implementation
 *
 * Implements the HNSW algorithm for approximate nearest neighbor search.
 * Reference: "Efficient and robust approximate nearest neighbor search using
 * Hierarchical Navigable Small World graphs" by Malkov & Yashunin (2018)
 *
 * @copyright MIT License
 */

#ifndef LYNX_HNSW_INDEX_H
#define LYNX_HNSW_INDEX_H

#include "../include/lynx/lynx.h"
#include "lynx_intern.h"
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <limits>
#include <queue>
#include <algorithm>
#include <cmath>

namespace lynx {

// ============================================================================
// VisitedTable - O(1) visited tracking with O(1) reset
// ============================================================================
// Instead of using std::unordered_set which requires allocations and has
// O(1) amortized but with high constant factor, we use a flat array with
// a visit counter. This is the same approach used by FAISS.
//
// - mark(idx): Set visited[idx] = current_visit_counter
// - is_visited(idx): Return visited[idx] == current_visit_counter
// - reset(): Just increment the counter (no need to clear the array)
//
// The counter wraps around at 255, at which point we clear the array.
// ============================================================================
class VisitedTable {
public:
    explicit VisitedTable(std::size_t size)
        : visited_(size, 0), visit_counter_(1) {}

    /// Mark a node as visited
    void mark(std::size_t idx) {
        if (idx < visited_.size()) {
            visited_[idx] = visit_counter_;
        }
    }

    /// Check if a node has been visited
    [[nodiscard]] bool is_visited(std::size_t idx) const {
        return idx < visited_.size() && visited_[idx] == visit_counter_;
    }

    /// Reset visited status for all nodes (O(1) operation)
    void reset() {
        if (++visit_counter_ == 0) {
            // Counter wrapped around, need to clear the array
            std::fill(visited_.begin(), visited_.end(), 0);
            visit_counter_ = 1;
        }
    }

    /// Resize the table (needed when adding new nodes)
    void resize(std::size_t new_size) {
        if (new_size > visited_.size()) {
            visited_.resize(new_size, 0);
        }
    }

    [[nodiscard]] std::size_t size() const { return visited_.size(); }

private:
    std::vector<uint8_t> visited_;
    uint8_t visit_counter_;
};

/**
 * @brief HNSW Index implementation.
 *
 * The HNSW index organizes vectors in a multi-layer hierarchical graph structure.
 * Each layer is a proximity graph where nodes are connected to their nearest neighbors.
 * The top layers are sparse with long-range connections for fast global navigation,
 * while the bottom layer is dense with all vectors for precise local search.
 *
 * Key properties:
 * - Query complexity: O(log N) expected
 * - Construction complexity: O(N * D * log N)
 * - Memory: O(N * M * avg_layers) for graph + O(N * D) for vectors
 *
 * Thread-safety: Concurrent reads are safe. Writes must be externally synchronized.
 */
class HNSWIndex : public IVectorIndex {
public:
    // -------------------------------------------------------------------------
    // Constructor and Destructor
    // -------------------------------------------------------------------------

    /**
     * @brief Construct HNSW index with configuration.
     * @param dimension Vector dimensionality
     * @param metric Distance metric to use
     * @param params HNSW-specific parameters
     */
    HNSWIndex(std::size_t dimension, DistanceMetric metric, const HNSWParams& params);

    ~HNSWIndex() override = default;

    // -------------------------------------------------------------------------
    // IVectorIndex Interface Implementation
    // -------------------------------------------------------------------------

    ErrorCode add(std::uint64_t id, std::span<const float> vector) override;
    ErrorCode remove(std::uint64_t id) override;
    [[nodiscard]] bool contains(std::uint64_t id) const override;

    [[nodiscard]] std::vector<SearchResultItem> search(
        std::span<const float> query,
        std::size_t k,
        const SearchParams& params) const override;

    ErrorCode build(std::span<const VectorRecord> vectors) override;

    ErrorCode serialize(std::ostream& out) const override;
    ErrorCode deserialize(std::istream& in) override;

    // -------------------------------------------------------------------------
    // Statistics and Metadata
    // -------------------------------------------------------------------------

    [[nodiscard]] std::size_t size() const { return id_to_index_.size(); }
    [[nodiscard]] std::size_t dimension() const { return dimension_; }
    [[nodiscard]] std::size_t memory_usage() const override;
    [[nodiscard]] std::size_t max_layer() const { return entry_point_layer_; }

    // -------------------------------------------------------------------------
    // Maintenance Operations
    // -------------------------------------------------------------------------

    /**
     * @brief Optimize the HNSW graph by pruning redundant edges.
     *
     * This method applies the RNG (Random Neighbor Graph) heuristic to remove
     * redundant edges in the graph. It improves graph quality, reduces memory
     * footprint, and can improve search performance by maintaining sparse yet
     * robust connectivity.
     *
     * The optimization process:
     * - Iterates through all nodes at all layers
     * - Applies heuristic pruning to remove edges that don't contribute to connectivity
     * - Preserves graph navigability
     *
     * Thread Safety: This operation requires write access and should not be
     * called concurrently with other write operations.
     *
     * @return ErrorCode::Ok on success, error code otherwise
     */
    ErrorCode optimize_graph();

    /**
     * @brief Compact the index by removing inconsistencies and validating integrity.
     *
     * This method performs index maintenance and cleanup:
     * - Removes dangling references in the graph (neighbors that no longer exist)
     * - Ensures consistency between graph_ and vectors_ data structures
     * - Validates and fixes the entry point if it's invalid
     * - Detects and repairs graph connectivity issues
     *
     * The compaction process is useful for:
     * - Recovering from potential corruption
     * - Cleaning up after many deletions
     * - Validating index integrity
     * - Preparing for future soft-delete support
     *
     * Thread Safety: This operation requires write access and should not be
     * called concurrently with other write operations.
     *
     * @return ErrorCode::Ok on success, error code otherwise
     */
    ErrorCode compact_index();

private:
    // -------------------------------------------------------------------------
    // Internal Data Structures
    // -------------------------------------------------------------------------

    /**
     * @brief A single node in the HNSW graph.
     *
     * Each node stores connections (neighbors) at each layer it participates in.
     * Layers are numbered from 0 (bottom, dense) to max_layer (top, sparse).
     */
    struct Node {
        std::uint64_t id;                                      ///< Vector identifier
        std::vector<std::unordered_set<std::uint64_t>> layers; ///< Neighbors per layer
        std::size_t max_layer;                                 ///< Highest layer this node is in

        explicit Node(std::uint64_t id, std::size_t max_layer)
            : id(id), layers(max_layer + 1), max_layer(max_layer) {}
    };

    /**
     * @brief Priority queue element for search operations.
     */
    struct Candidate {
        std::uint64_t id;
        float distance;

        bool operator<(const Candidate& other) const {
            return distance < other.distance; // Min-heap
        }

        bool operator>(const Candidate& other) const {
            return distance > other.distance; // Max-heap
        }
    };

    // -------------------------------------------------------------------------
    // Core HNSW Algorithms
    // -------------------------------------------------------------------------

    /**
     * @brief Generate random layer for a new node using exponential decay.
     *
     * The layer is selected with probability: P(layer=l) = (1/ml)^l
     * where ml is the normalization factor (typically 1/ln(M)).
     *
     * @return Layer number [0, max_layer]
     */
    std::size_t generate_random_layer();

    /**
     * @brief Search for nearest neighbors at a specific layer.
     *
     * Performs greedy search starting from entry points, navigating the
     * graph to find the ef nearest neighbors at the given layer.
     *
     * @param query Query vector
     * @param entry_points Starting nodes for search
     * @param ef Number of neighbors to explore
     * @param layer Layer to search in
     * @return Vector of (id, distance) candidates, sorted by distance ascending
     */
    [[nodiscard]] std::vector<Candidate> search_layer(
        std::span<const float> query,
        const std::vector<std::uint64_t>& entry_points,
        std::size_t ef,
        std::size_t layer) const;

    /**
     * @brief Select M neighbors from candidates using heuristic pruning.
     *
     * Implements the heuristic neighbor selection strategy from the paper.
     * Prioritizes closer neighbors and avoids redundant connections.
     *
     * @param candidates Set of candidate neighbors (id, distance)
     * @param m Maximum number of neighbors to select
     * @param layer Layer being processed
     * @param extend_candidates Whether to include existing neighbors as candidates
     * @return Selected neighbor IDs
     */
    std::vector<std::uint64_t> select_neighbors_heuristic(
        std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>>& candidates,
        std::size_t m,
        std::size_t layer,
        bool extend_candidates = false);

    /**
     * @brief Select M neighbors from candidates using simple pruning.
     *
     * Simple strategy: just select the M closest neighbors.
     *
     * @param candidates Set of candidate neighbors (id, distance)
     * @param m Maximum number of neighbors to select
     * @return Selected neighbor IDs
     */
    std::vector<std::uint64_t> select_neighbors_simple(
        std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>>& candidates,
        std::size_t m);

    /**
     * @brief Add bidirectional connection between two nodes at a layer.
     *
     * @param source Source node ID
     * @param target Target node ID
     * @param layer Layer to add connection
     */
    void add_connection(std::uint64_t source, std::uint64_t target, std::size_t layer);

    /**
     * @brief Prune connections if node exceeds max connections.
     *
     * @param node_id Node to prune
     * @param layer Layer to prune
     * @param max_connections Maximum allowed connections
     */
    void prune_connections(std::uint64_t node_id, std::size_t layer, std::size_t max_connections);

    /**
     * @brief Calculate distance between query and a stored vector.
     *
     * @param query Query vector
     * @param id ID of stored vector
     * @return Distance value
     */
    [[nodiscard]] float calculate_distance(std::span<const float> query, std::uint64_t id) const;

    /**
     * @brief Calculate distance between two stored vectors.
     *
     * @param id1 First vector ID
     * @param id2 Second vector ID
     * @return Distance value
     */
    [[nodiscard]] float calculate_distance(std::uint64_t id1, std::uint64_t id2) const;

    /**
     * @brief Get a span to the vector data for a given index.
     *
     * @param index Vector index (not ID)
     * @return Span to the vector data
     */
    [[nodiscard]] std::span<const float> get_vector_by_index(std::size_t index) const {
        return std::span<const float>(vector_data_.data() + index * dimension_, dimension_);
    }

    /**
     * @brief Get a span to the vector data for a given ID.
     *
     * @param id Vector ID
     * @return Span to the vector data, or empty span if not found
     */
    [[nodiscard]] std::span<const float> get_vector_by_id(std::uint64_t id) const {
        auto it = id_to_index_.find(id);
        if (it == id_to_index_.end()) {
            return {};
        }
        return get_vector_by_index(it->second);
    }

    /**
     * @brief Get the internal index for a given ID.
     *
     * @param id Vector ID
     * @return Index, or SIZE_MAX if not found
     */
    [[nodiscard]] std::size_t get_index_for_id(std::uint64_t id) const {
        auto it = id_to_index_.find(id);
        return it != id_to_index_.end() ? it->second : std::numeric_limits<std::size_t>::max();
    }

    /**
     * @brief Get the neighbors of a node at a specific layer.
     *
     * @param id Node ID
     * @param layer Layer number
     * @return Set of neighbor IDs (empty if node doesn't exist)
     */
    [[nodiscard]] const std::unordered_set<std::uint64_t>& get_neighbors(
        std::uint64_t id, std::size_t layer) const;

    /**
     * @brief Fast greedy descent through upper layers.
     *
     * Performs a simple greedy walk from start_node at start_layer down to
     * target_layer, following the nearest neighbor at each layer.
     * This is faster than calling search_layer at each level.
     *
     * @param query Query vector
     * @param start_node Starting node ID
     * @param start_layer Starting layer (highest)
     * @param target_layer Target layer to descend to
     * @return Best entry point node ID for the target layer
     */
    [[nodiscard]] std::uint64_t greedy_descent(
        std::span<const float> query,
        std::uint64_t start_node,
        std::size_t start_layer,
        std::size_t target_layer) const;

    // -------------------------------------------------------------------------
    // Member Variables
    // -------------------------------------------------------------------------

    std::size_t dimension_;                                    ///< Vector dimensionality
    DistanceMetric metric_;                                     ///< Distance metric
    HNSWParams params_;                                         ///< HNSW configuration

    // Graph structure
    std::unordered_map<std::uint64_t, Node> graph_;            ///< Graph nodes (id -> Node)

    // Contiguous vector storage for cache-efficient distance calculations
    // Instead of std::unordered_map<id, vector<float>>, we store all vectors
    // contiguously and use an ID-to-index mapping for lookups.
    std::vector<float> vector_data_;                           ///< Contiguous vector data
    std::unordered_map<std::uint64_t, std::size_t> id_to_index_; ///< ID to vector index mapping
    std::vector<std::uint64_t> index_to_id_;                   ///< Index to ID mapping (for VisitedTable)

    // Entry point tracking
    std::uint64_t entry_point_;                                 ///< Entry node ID (top layer)
    std::size_t entry_point_layer_;                             ///< Maximum layer in graph

    // Layer generation
    std::mt19937_64 rng_;                                       ///< Random number generator
    std::uniform_real_distribution<double> level_dist_;         ///< Uniform [0,1) for layer generation
    double ml_;                                                 ///< Layer multiplier (1/ln(M))

    // Thread safety
    mutable std::shared_mutex mutex_;                           ///< Reader-writer lock

    // Reusable visited table for search operations (mutable for const methods)
    mutable VisitedTable visited_table_;                        ///< Visited tracking for searches

    // Constants
    static constexpr std::uint64_t kInvalidId = std::numeric_limits<std::uint64_t>::max();
    static constexpr std::size_t kDefaultEfConstruction = 200;
    static const std::unordered_set<std::uint64_t> kEmptyNeighborSet;
};

} // namespace lynx

#endif // LYNX_HNSW_INDEX_H
