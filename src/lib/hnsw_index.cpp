/**
 * @file hnsw_index.cpp
 * @brief HNSW Index Implementation
 *
 * @copyright MIT License
 */

#include "hnsw_index.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <mutex>

namespace lynx {

// Static member initialization
const std::unordered_set<std::uint64_t> HNSWIndex::kEmptyNeighborSet;

// Forward declarations for distance metric functions
namespace {

/**
 * @brief Calculate L2 (Euclidean) distance between two vectors.
 */
float calculate_l2_distance(std::span<const float> a, std::span<const float> b) {
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

/**
 * @brief Calculate cosine distance between two vectors.
 */
float calculate_cosine_distance(std::span<const float> a, std::span<const float> b) {
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a == 0.0f || norm_b == 0.0f) {
        return 1.0f; // Maximum distance for zero vectors
    }

    return 1.0f - (dot / (std::sqrt(norm_a) * std::sqrt(norm_b)));
}

/**
 * @brief Calculate dot product distance between two vectors.
 */
float calculate_dot_product_distance(std::span<const float> a, std::span<const float> b) {
    float dot = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
    }
    return -dot; // Negative because we want smaller values for closer vectors
}

/**
 * @brief Generic distance calculation dispatcher.
 */
[[maybe_unused]]
float calculate_distance(std::span<const float> a, std::span<const float> b, DistanceMetric metric) {
    switch (metric) {
        case DistanceMetric::L2:
            return calculate_l2_distance(a, b);
        case DistanceMetric::Cosine:
            return calculate_cosine_distance(a, b);
        case DistanceMetric::DotProduct:
            return calculate_dot_product_distance(a, b);
        default:
            return calculate_l2_distance(a, b);
    }
}

} // anonymous namespace

// ============================================================================
// Constructor
// ============================================================================

HNSWIndex::HNSWIndex(std::size_t dimension, DistanceMetric metric, const HNSWParams& params)
    : dimension_(dimension)
    , metric_(metric)
    , params_(params)
    , entry_point_(kInvalidId)
    , entry_point_layer_(0)
    , rng_(std::random_device{}())
    , level_dist_(0.0, 1.0)
    , ml_(1.0 / std::log(params.m)) {
}

// ============================================================================
// Layer Generation
// ============================================================================

std::size_t HNSWIndex::generate_random_layer() {
    // Generate layer using exponential decay probability distribution
    // P(layer = l) = (1/ml)^l where ml = 1/ln(M)
    const double r = level_dist_(rng_);
    if (r <= std::numeric_limits<double>::epsilon()) {
        return 0;
    }

    // layer = floor(-ln(r) * ml)
    const std::size_t layer = static_cast<std::size_t>(-std::log(r) * ml_);

    // Cap at reasonable maximum to prevent excessive memory usage
    constexpr std::size_t kMaxLayer = 16;
    return std::min(layer, kMaxLayer);
}

// ============================================================================
// Distance Calculation
// ============================================================================

float HNSWIndex::calculate_distance(std::span<const float> query, std::uint64_t id) const {
    const auto it = vectors_.find(id);
    if (it == vectors_.end()) {
        return std::numeric_limits<float>::max();
    }
    return ::lynx::calculate_distance(query, it->second, metric_);
}

float HNSWIndex::calculate_distance(std::uint64_t id1, std::uint64_t id2) const {
    const auto it1 = vectors_.find(id1);
    const auto it2 = vectors_.find(id2);
    if (it1 == vectors_.end() || it2 == vectors_.end()) {
        return std::numeric_limits<float>::max();
    }
    return ::lynx::calculate_distance(it1->second, it2->second, metric_);
}

// ============================================================================
// Graph Operations
// ============================================================================

const std::unordered_set<std::uint64_t>& HNSWIndex::get_neighbors(
    std::uint64_t id, std::size_t layer) const {
    const auto it = graph_.find(id);
    if (it == graph_.end() || layer > it->second.max_layer) {
        return kEmptyNeighborSet;
    }
    return it->second.layers[layer];
}

void HNSWIndex::add_connection(std::uint64_t source, std::uint64_t target, std::size_t layer) {
    auto& source_node = graph_.at(source);
    auto& target_node = graph_.at(target);

    if (layer <= source_node.max_layer) {
        source_node.layers[layer].insert(target);
    }
    if (layer <= target_node.max_layer) {
        target_node.layers[layer].insert(source);
    }
}

void HNSWIndex::prune_connections(std::uint64_t node_id, std::size_t layer, std::size_t max_connections) {
    auto& node = graph_.at(node_id);
    auto& neighbors = node.layers[layer];

    if (neighbors.size() <= max_connections) {
        return; // No pruning needed
    }

    // Build candidates from current neighbors
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> candidates;
    for (auto neighbor_id : neighbors) {
        const float dist = calculate_distance(node_id, neighbor_id);
        candidates.push({neighbor_id, dist});
    }

    // Select best neighbors using heuristic
    auto selected = select_neighbors_heuristic(candidates, max_connections, layer, false);

    // Update connections
    neighbors.clear();
    for (auto id : selected) {
        neighbors.insert(id);
    }
}

// ============================================================================
// Search Layer Algorithm
// ============================================================================

std::priority_queue<HNSWIndex::Candidate, std::vector<HNSWIndex::Candidate>, std::less<HNSWIndex::Candidate>>
HNSWIndex::search_layer(
    std::span<const float> query,
    const std::vector<std::uint64_t>& entry_points,
    std::size_t ef,
    std::size_t layer) const {

    // Visited set to avoid processing nodes multiple times
    std::unordered_set<std::uint64_t> visited;

    // Candidates: min-heap by distance (closest first)
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> candidates;

    // Dynamic list: max-heap by distance (farthest first)
    std::priority_queue<Candidate, std::vector<Candidate>, std::less<Candidate>> w;

    // Initialize with entry points
    for (auto ep_id : entry_points) {
        const float dist = calculate_distance(query, ep_id);
        candidates.push({ep_id, dist});
        w.push({ep_id, dist});
        visited.insert(ep_id);
    }

    // Greedy search
    while (!candidates.empty()) {
        const Candidate current = candidates.top();
        candidates.pop();

        // If current is farther than farthest in result set, stop
        if (current.distance > w.top().distance) {
            break;
        }

        // Explore neighbors
        const auto& neighbors = get_neighbors(current.id, layer);
        for (auto neighbor_id : neighbors) {
            if (visited.find(neighbor_id) == visited.end()) {
                visited.insert(neighbor_id);

                const float dist = calculate_distance(query, neighbor_id);

                // If better than worst in result set, or result set not full
                if (dist < w.top().distance || w.size() < ef) {
                    candidates.push({neighbor_id, dist});
                    w.push({neighbor_id, dist});

                    // Keep only ef best candidates
                    if (w.size() > ef) {
                        w.pop();
                    }
                }
            }
        }
    }

    return w;
}

// ============================================================================
// Neighbor Selection Algorithms
// ============================================================================

std::vector<std::uint64_t> HNSWIndex::select_neighbors_simple(
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>>& candidates,
    std::size_t m) {

    std::vector<std::uint64_t> selected;
    selected.reserve(m);

    while (!candidates.empty() && selected.size() < m) {
        selected.push_back(candidates.top().id);
        candidates.pop();
    }

    return selected;
}

std::vector<std::uint64_t> HNSWIndex::select_neighbors_heuristic(
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>>& candidates,
    std::size_t m,
    [[maybe_unused]] std::size_t layer,
    bool extend_candidates) {

    std::vector<std::uint64_t> result;
    result.reserve(m);

    // Working queue (min-heap by distance)
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> w;

    // Copy candidates to working queue
    while (!candidates.empty()) {
        w.push(candidates.top());
        candidates.pop();
    }

    // If extending candidates, add neighbors of candidates
    if (extend_candidates) {
        // For now, skip extension to keep it simple
        // Can be added later for improved quality
    }

    // Discarded candidates (max-heap by distance)
    std::priority_queue<Candidate, std::vector<Candidate>, std::less<Candidate>> w_d;

    // Heuristic selection: prioritize closer nodes and avoid redundancy
    while (!w.empty() && result.size() < m) {
        Candidate current = w.top();
        w.pop();

        // Check if current is closer to query than to any selected neighbor
        bool good = true;

        if (!result.empty()) {
            const float dist_to_query = current.distance;

            // Check distance to already selected neighbors
            for (auto selected_id : result) {
                const float dist_to_selected = calculate_distance(current.id, selected_id);

                // If current is closer to a selected neighbor than to query,
                // it might be redundant
                if (dist_to_selected < dist_to_query) {
                    good = false;
                    break;
                }
            }
        }

        if (good) {
            result.push_back(current.id);
        } else {
            w_d.push(current);
        }
    }

    // If we don't have enough, add from discarded
    while (!w_d.empty() && result.size() < m) {
        result.push_back(w_d.top().id);
        w_d.pop();
    }

    return result;
}

// ============================================================================
// Insert Algorithm
// ============================================================================

ErrorCode HNSWIndex::add(std::uint64_t id, std::span<const float> vector) {
    std::unique_lock lock(mutex_);

    // Validate dimension
    if (vector.size() != dimension_) {
        return ErrorCode::DimensionMismatch;
    }

    // Check if already exists
    if (vectors_.find(id) != vectors_.end()) {
        return ErrorCode::InvalidState;
    }

    // Store vector
    vectors_[id] = std::vector<float>(vector.begin(), vector.end());

    // Generate random layer for new node
    const std::size_t node_layer = generate_random_layer();

    // Create node
    graph_.emplace(id, Node(id, node_layer));

    // If this is the first node, make it the entry point
    if (entry_point_ == kInvalidId) {
        entry_point_ = id;
        entry_point_layer_ = node_layer;
        return ErrorCode::Ok;
    }

    // Find nearest neighbors at each layer
    std::vector<std::uint64_t> entry_points = {entry_point_};

    // Search from top to target layer + 1
    for (std::size_t lc = entry_point_layer_; lc > node_layer; --lc) {
        auto nearest = search_layer(vector, entry_points, 1, lc);
        if (!nearest.empty()) {
            entry_points = {nearest.top().id};
        }
    }

    // Insert at layers from node_layer down to 0
    for (std::size_t lc = std::min(node_layer, entry_point_layer_); ; --lc) {
        // Find ef_construction nearest neighbors at this layer
        auto candidates = search_layer(vector, entry_points, params_.ef_construction, lc);

        // Convert to min-heap for neighbor selection
        std::priority_queue<Candidate, std::vector<Candidate>, std::greater<Candidate>> candidates_min;
        while (!candidates.empty()) {
            candidates_min.push(candidates.top());
            candidates.pop();
        }

        // Select M neighbors
        const std::size_t m = (lc == 0) ? (2 * params_.m) : params_.m;
        auto neighbors = select_neighbors_heuristic(candidates_min, m, lc, false);

        // Add bidirectional connections
        for (auto neighbor_id : neighbors) {
            add_connection(id, neighbor_id, lc);

            // Prune neighbor's connections if needed
            const std::size_t max_conn = (lc == 0) ? (2 * params_.m) : params_.m;
            prune_connections(neighbor_id, lc, max_conn);
        }

        // Update entry points for next layer
        entry_points.clear();
        for (auto neighbor_id : neighbors) {
            entry_points.push_back(neighbor_id);
        }

        if (lc == 0) break;
    }

    // Update entry point if new node is higher
    if (node_layer > entry_point_layer_) {
        entry_point_ = id;
        entry_point_layer_ = node_layer;
    }

    return ErrorCode::Ok;
}

// ============================================================================
// Search Algorithm
// ============================================================================

std::vector<SearchResultItem> HNSWIndex::search(
    std::span<const float> query,
    std::size_t k,
    const SearchParams& params) const {

    std::shared_lock lock(mutex_);

    // Validate dimension
    if (query.size() != dimension_) {
        return {};
    }

    // Check if index is empty
    if (entry_point_ == kInvalidId) {
        return {};
    }

    // Start from entry point
    std::vector<std::uint64_t> entry_points = {entry_point_};

    // Search from top layer to layer 1
    for (std::size_t lc = entry_point_layer_; lc > 0; --lc) {
        auto nearest = search_layer(query, entry_points, 1, lc);
        if (!nearest.empty()) {
            entry_points = {nearest.top().id};
        }
    }

    // Search at layer 0 with ef_search
    const std::size_t ef_search = params.ef_search > 0 ? params.ef_search : params_.ef_search;
    auto candidates = search_layer(query, entry_points, std::max(ef_search, k), 0);

    // Extract top k results
    std::vector<SearchResultItem> results;
    results.reserve(std::min(k, candidates.size()));

    // Collect results in reverse order (we want closest first)
    std::vector<Candidate> temp;
    while (!candidates.empty()) {
        temp.push_back(candidates.top());
        candidates.pop();
    }

    // Reverse and filter
    std::reverse(temp.begin(), temp.end());
    for (std::size_t i = 0; i < std::min(k, temp.size()); ++i) {
        // Apply filter if provided
        if (params.filter && !(*params.filter)(temp[i].id)) {
            continue;
        }
        results.push_back({temp[i].id, temp[i].distance});
    }

    // If filter was applied, we might have fewer than k results
    // That's acceptable for filtered searches

    return results;
}

// ============================================================================
// Remove Algorithm
// ============================================================================

ErrorCode HNSWIndex::remove(std::uint64_t id) {
    std::unique_lock lock(mutex_);

    // Check if exists
    auto vec_it = vectors_.find(id);
    if (vec_it == vectors_.end()) {
        return ErrorCode::VectorNotFound;
    }

    auto graph_it = graph_.find(id);
    if (graph_it == graph_.end()) {
        return ErrorCode::InvalidState;
    }

    const Node& node = graph_it->second;

    // Remove all connections to this node
    for (std::size_t layer = 0; layer <= node.max_layer; ++layer) {
        const auto& neighbors = node.layers[layer];
        for (auto neighbor_id : neighbors) {
            auto neighbor_it = graph_.find(neighbor_id);
            if (neighbor_it != graph_.end() && layer < neighbor_it->second.layers.size()) {
                neighbor_it->second.layers[layer].erase(id);
            }
        }
    }

    // Remove from graph and vectors
    graph_.erase(graph_it);
    vectors_.erase(vec_it);

    // Update entry point if needed
    if (id == entry_point_) {
        // Find new entry point (node with highest layer)
        entry_point_ = kInvalidId;
        entry_point_layer_ = 0;

        for (const auto& [node_id, node] : graph_) {
            if (node.max_layer > entry_point_layer_) {
                entry_point_ = node_id;
                entry_point_layer_ = node.max_layer;
            }
        }
    }

    return ErrorCode::Ok;
}

// ============================================================================
// Utility Methods
// ============================================================================

bool HNSWIndex::contains(std::uint64_t id) const {
    std::shared_lock lock(mutex_);
    return vectors_.find(id) != vectors_.end();
}

std::size_t HNSWIndex::memory_usage() const {
    std::shared_lock lock(mutex_);

    std::size_t total = 0;

    // Vector storage: vectors_ map
    for (const auto& [id, vec] : vectors_) {
        total += sizeof(id);                    // Key
        total += sizeof(float) * vec.size();    // Vector data
        total += sizeof(vec);                   // Vector object overhead
    }

    // Graph storage: graph_ map
    for (const auto& [id, node] : graph_) {
        total += sizeof(id);                    // Key
        total += sizeof(node);                  // Node object
        for (const auto& layer : node.layers) {
            total += sizeof(layer);             // Set object
            total += layer.size() * sizeof(std::uint64_t); // Set elements
        }
    }

    // Don't include fixed object overhead (sizeof(*this))
    // Only count dynamic allocations (vectors and graph nodes)

    return total;
}

ErrorCode HNSWIndex::build(std::span<const VectorRecord> vectors) {
    // Build index from batch of vectors
    for (const auto& record : vectors) {
        ErrorCode err = add(record.id, record.vector);
        if (err != ErrorCode::Ok) {
            return err;
        }
    }
    return ErrorCode::Ok;
}

// ============================================================================
// Graph Optimization
// ============================================================================

ErrorCode HNSWIndex::optimize_graph() {
    std::unique_lock lock(mutex_);

    // If index is empty or too small, no optimization needed
    if (graph_.empty() || graph_.size() < 10) {
        return ErrorCode::Ok;
    }

    // Statistics tracking
    std::size_t total_edges_before = 0;
    std::size_t total_edges_after = 0;

    // Count edges before optimization
    for (const auto& [id, node] : graph_) {
        for (std::size_t layer = 0; layer <= node.max_layer; ++layer) {
            total_edges_before += node.layers[layer].size();
        }
    }

    // Iterate through all nodes and prune connections at each layer
    // We use a copy of node IDs to avoid iterator invalidation
    std::vector<std::uint64_t> node_ids;
    node_ids.reserve(graph_.size());
    for (const auto& [id, _] : graph_) {
        node_ids.push_back(id);
    }

    for (auto node_id : node_ids) {
        auto node_it = graph_.find(node_id);
        if (node_it == graph_.end()) {
            continue; // Node might have been removed
        }

        const Node& node = node_it->second;

        // Prune connections at each layer
        for (std::size_t layer = 0; layer <= node.max_layer; ++layer) {
            // Determine max connections for this layer
            // Layer 0 allows 2*M connections, other layers allow M connections
            const std::size_t max_connections = (layer == 0) ? (2 * params_.m) : params_.m;

            // Only prune if we have more than the minimum threshold
            // We use M/2 as a threshold to avoid pruning well-connected nodes
            const std::size_t min_threshold = params_.m / 2;
            const auto& neighbors = node.layers[layer];

            // Only optimize if node has significantly more connections than needed
            if (neighbors.size() > max_connections || neighbors.size() < min_threshold) {
                prune_connections(node_id, layer, max_connections);
            }
        }
    }

    // Count edges after optimization
    for (const auto& [id, node] : graph_) {
        for (std::size_t layer = 0; layer <= node.max_layer; ++layer) {
            total_edges_after += node.layers[layer].size();
        }
    }

    // Log optimization results (optional - can be removed if no logging infrastructure)
    // For now, we just silently complete the optimization
    // In production, you might want to track these statistics

    return ErrorCode::Ok;
}

// ============================================================================
// Serialization (Placeholder)
// ============================================================================

ErrorCode HNSWIndex::serialize(std::ostream& out) const {
    std::shared_lock lock(mutex_);

    try {
        // Write magic number and version for file format verification
        constexpr uint32_t kMagicNumber = 0x484E5357; // "HNSW" in hex
        constexpr uint32_t kVersion = 1;

        out.write(reinterpret_cast<const char*>(&kMagicNumber), sizeof(kMagicNumber));
        out.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));

        // Write configuration
        out.write(reinterpret_cast<const char*>(&dimension_), sizeof(dimension_));

        uint8_t metric_value = static_cast<uint8_t>(metric_);
        out.write(reinterpret_cast<const char*>(&metric_value), sizeof(metric_value));

        out.write(reinterpret_cast<const char*>(&params_.m), sizeof(params_.m));
        out.write(reinterpret_cast<const char*>(&params_.ef_construction), sizeof(params_.ef_construction));
        out.write(reinterpret_cast<const char*>(&params_.ef_search), sizeof(params_.ef_search));
        out.write(reinterpret_cast<const char*>(&params_.max_elements), sizeof(params_.max_elements));

        // Write entry point information
        out.write(reinterpret_cast<const char*>(&entry_point_), sizeof(entry_point_));
        out.write(reinterpret_cast<const char*>(&entry_point_layer_), sizeof(entry_point_layer_));

        // Write number of vectors
        size_t num_vectors = vectors_.size();
        out.write(reinterpret_cast<const char*>(&num_vectors), sizeof(num_vectors));

        // Write each vector and its graph structure
        for (const auto& [id, vector] : vectors_) {
            // Write vector ID
            out.write(reinterpret_cast<const char*>(&id), sizeof(id));

            // Write vector data
            out.write(reinterpret_cast<const char*>(vector.data()),
                     vector.size() * sizeof(float));

            // Write node information
            auto node_it = graph_.find(id);
            if (node_it == graph_.end()) {
                return ErrorCode::InvalidState;
            }

            const Node& node = node_it->second;
            out.write(reinterpret_cast<const char*>(&node.max_layer), sizeof(node.max_layer));

            // Write neighbors for each layer
            for (size_t layer = 0; layer <= node.max_layer; ++layer) {
                const auto& neighbors = node.layers[layer];
                size_t num_neighbors = neighbors.size();
                out.write(reinterpret_cast<const char*>(&num_neighbors), sizeof(num_neighbors));

                for (uint64_t neighbor_id : neighbors) {
                    out.write(reinterpret_cast<const char*>(&neighbor_id), sizeof(neighbor_id));
                }
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

ErrorCode HNSWIndex::deserialize(std::istream& in) {
    std::unique_lock lock(mutex_);

    try {
        // Read and verify magic number
        constexpr uint32_t kExpectedMagic = 0x484E5357; // "HNSW"
        uint32_t magic_number;
        in.read(reinterpret_cast<char*>(&magic_number), sizeof(magic_number));
        if (magic_number != kExpectedMagic) {
            return ErrorCode::IOError; // Invalid file format
        }

        // Read and verify version
        uint32_t version;
        in.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != 1) {
            return ErrorCode::IOError; // Unsupported version
        }

        // Read configuration
        size_t dimension;
        in.read(reinterpret_cast<char*>(&dimension), sizeof(dimension));
        if (dimension != dimension_) {
            return ErrorCode::DimensionMismatch;
        }

        uint8_t metric_value;
        in.read(reinterpret_cast<char*>(&metric_value), sizeof(metric_value));
        metric_ = static_cast<DistanceMetric>(metric_value);

        in.read(reinterpret_cast<char*>(&params_.m), sizeof(params_.m));
        in.read(reinterpret_cast<char*>(&params_.ef_construction), sizeof(params_.ef_construction));
        in.read(reinterpret_cast<char*>(&params_.ef_search), sizeof(params_.ef_search));
        in.read(reinterpret_cast<char*>(&params_.max_elements), sizeof(params_.max_elements));

        // Recalculate ml_ based on loaded params
        ml_ = 1.0 / std::log(params_.m);

        // Read entry point information
        in.read(reinterpret_cast<char*>(&entry_point_), sizeof(entry_point_));
        in.read(reinterpret_cast<char*>(&entry_point_layer_), sizeof(entry_point_layer_));

        // Read number of vectors
        size_t num_vectors;
        in.read(reinterpret_cast<char*>(&num_vectors), sizeof(num_vectors));

        // Clear existing data
        vectors_.clear();
        graph_.clear();

        // Read each vector and its graph structure
        for (size_t i = 0; i < num_vectors; ++i) {
            // Read vector ID
            uint64_t id;
            in.read(reinterpret_cast<char*>(&id), sizeof(id));

            // Read vector data
            std::vector<float> vector(dimension);
            in.read(reinterpret_cast<char*>(vector.data()),
                   vector.size() * sizeof(float));
            vectors_[id] = std::move(vector);

            // Read node information
            size_t max_layer;
            in.read(reinterpret_cast<char*>(&max_layer), sizeof(max_layer));

            // Create node
            Node node(id, max_layer);

            // Read neighbors for each layer
            for (size_t layer = 0; layer <= max_layer; ++layer) {
                size_t num_neighbors;
                in.read(reinterpret_cast<char*>(&num_neighbors), sizeof(num_neighbors));

                for (size_t j = 0; j < num_neighbors; ++j) {
                    uint64_t neighbor_id;
                    in.read(reinterpret_cast<char*>(&neighbor_id), sizeof(neighbor_id));
                    node.layers[layer].insert(neighbor_id);
                }
            }

            graph_.emplace(id, std::move(node));
        }

        if (!in.good()) {
            // Restore to empty state on error
            vectors_.clear();
            graph_.clear();
            entry_point_ = kInvalidId;
            entry_point_layer_ = 0;
            return ErrorCode::IOError;
        }

        return ErrorCode::Ok;

    } catch (const std::exception&) {
        // Restore to empty state on exception
        vectors_.clear();
        graph_.clear();
        entry_point_ = kInvalidId;
        entry_point_layer_ = 0;
        return ErrorCode::IOError;
    }
}

} // namespace lynx
