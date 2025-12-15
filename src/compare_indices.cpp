/**
 * @file compare_indices.cpp
 * @brief Compare search results between Flat, HNSW, and IVF indices
 */

#include "lynx/lynx.h"
#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <set>
#include <chrono>

static constexpr size_t NUM_VECTORS = 2000;  // Number of vectors to insert and compare

// Helper function to generate random vectors
std::vector<float> generate_random_vector(size_t dim, std::mt19937& gen) {
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    std::vector<float> vec(dim);
    for (size_t i = 0; i < dim; ++i) {
        vec[i] = dis(gen);
    }
    return vec;
}

void compare_search_results(const std::string& title,
                            const lynx::SearchResult& flat_result,
                            const lynx::SearchResult& hnsw_result,
                            const lynx::SearchResult& ivf_result,
                            size_t k) {
    std::cout << "\n" << title << "\n";
    std::cout << std::string(80, '=') << "\n";

    std::set<uint64_t> flat_ids;
    for (const auto& item : flat_result.items) {
        flat_ids.insert(item.id);
    }

    std::set<uint64_t> hnsw_ids;
    for (const auto& item : hnsw_result.items) {
        hnsw_ids.insert(item.id);
    }

    std::set<uint64_t> ivf_ids;
    for (const auto& item : ivf_result.items) {
        ivf_ids.insert(item.id);
    }

    // Count matches with Flat (ground truth)
    size_t hnsw_matched = 0;
    for (auto id : hnsw_ids) {
        if (flat_ids.find(id) != flat_ids.end()) {
            hnsw_matched++;
        }
    }

    size_t ivf_matched = 0;
    for (auto id : ivf_ids) {
        if (flat_ids.find(id) != flat_ids.end()) {
            ivf_matched++;
        }
    }

    std::cout << "Recall vs Flat (ground truth):\n";
    std::cout << "  HNSW: " << hnsw_matched << " / " << k << " ("
              << std::fixed << std::setprecision(1)
              << (100.0 * hnsw_matched / k) << "%)\n";
    std::cout << "  IVF:  " << ivf_matched << " / " << k << " ("
              << std::fixed << std::setprecision(1)
              << (100.0 * ivf_matched / k) << "%)\n";

    // Compare top results side-by-side
    std::cout << "\nRank | Flat Index          | HNSW Index          | IVF Index           |\n";
    std::cout << "-----+---------------------+---------------------+---------------------+\n";

    for (size_t i = 0; i < k; ++i) {
        std::cout << std::setw(4) << (i + 1) << " | ";

        // Flat result
        if (i < flat_result.items.size()) {
            std::cout << "ID=" << std::setw(5) << flat_result.items[i].id
                      << " D=" << std::setw(6) << std::setprecision(4)
                      << flat_result.items[i].distance;
        } else {
            std::cout << std::setw(19) << "-";
        }

        std::cout << " | ";

        // HNSW result
        if (i < hnsw_result.items.size()) {
            std::cout << "ID=" << std::setw(5) << hnsw_result.items[i].id
                      << " D=" << std::setw(6) << std::setprecision(4)
                      << hnsw_result.items[i].distance;
        } else {
            std::cout << std::setw(19) << "-";
        }

        std::cout << " | ";

        // IVF result
        if (i < ivf_result.items.size()) {
            std::cout << "ID=" << std::setw(5) << ivf_result.items[i].id
                      << " D=" << std::setw(6) << std::setprecision(4)
                      << ivf_result.items[i].distance;
        } else {
            std::cout << std::setw(19) << "-";
        }

        std::cout << " |\n";
    }

    std::cout << "\nQuery Time:\n";
    std::cout << "  Flat: " << std::setprecision(3) << flat_result.query_time_ms << " ms\n";
    std::cout << "  HNSW: " << std::setprecision(3) << hnsw_result.query_time_ms << " ms";
    if (flat_result.query_time_ms > 0) {
        std::cout << " (" << std::setprecision(2)
                  << (flat_result.query_time_ms / std::max(hnsw_result.query_time_ms, 0.001))
                  << "x speedup)";
    }
    std::cout << "\n";
    std::cout << "  IVF:  " << std::setprecision(3) << ivf_result.query_time_ms << " ms";
    if (flat_result.query_time_ms > 0) {
        std::cout << " (" << std::setprecision(2)
                  << (flat_result.query_time_ms / std::max(ivf_result.query_time_ms, 0.001))
                  << "x speedup)";
    }
    std::cout << "\n";
}

int main() {
    std::cout << "========================================================================\n";
    std::cout << "Lynx Vector Database - Flat vs HNSW vs IVF Comparison\n";
    std::cout << "========================================================================\n\n";

    // Fixed seed for reproducibility
    std::mt19937 gen(42);

    const size_t dimension = 512;
    const size_t num_vectors = NUM_VECTORS;
    const size_t k = 10;

    // Configuration
    lynx::Config flat_config;
    flat_config.dimension = dimension;
    flat_config.index_type = lynx::IndexType::Flat;
    flat_config.distance_metric = lynx::DistanceMetric::L2;

    lynx::Config hnsw_config = flat_config;
    hnsw_config.index_type = lynx::IndexType::HNSW;
    hnsw_config.hnsw_params.m = 16;
    hnsw_config.hnsw_params.ef_construction = 200;
    hnsw_config.hnsw_params.ef_search = 50;
    hnsw_config.hnsw_params.random_seed = 123;  // Fixed seed for reproducibility

    lynx::Config ivf_config = flat_config;
    ivf_config.index_type = lynx::IndexType::IVF;
    ivf_config.ivf_params.n_clusters = 100;  // For 1000 vectors, ~10 vectors per cluster
    ivf_config.ivf_params.n_probe = 10;      // Search 10 clusters

    std::cout << "Setup:\n";
    std::cout << "  Dimension: " << dimension << "\n";
    std::cout << "  Num vectors: " << num_vectors << "\n";
    std::cout << "  k (neighbors): " << k << "\n";
    std::cout << "  Distance metric: L2\n";
    std::cout << "  HNSW parameters: M=" << hnsw_config.hnsw_params.m
              << ", ef_construction=" << hnsw_config.hnsw_params.ef_construction
              << ", ef_search=" << hnsw_config.hnsw_params.ef_search << "\n";
    std::cout << "  IVF parameters: n_clusters=" << ivf_config.ivf_params.n_clusters
              << ", n_probe=" << ivf_config.ivf_params.n_probe << "\n\n";

    // Create databases
    auto flat_db = lynx::IVectorDatabase::create(flat_config);
    auto hnsw_db = lynx::IVectorDatabase::create(hnsw_config);
    auto ivf_db = lynx::IVectorDatabase::create(ivf_config);

    // Generate all vectors first (for fair comparison)
    std::cout << "Generating " << num_vectors << " vectors...\n";
    std::vector<std::vector<float>> all_vectors;
    all_vectors.reserve(num_vectors);
    for (uint64_t id = 1; id <= num_vectors; ++id) {
        all_vectors.push_back(generate_random_vector(dimension, gen));
    }

    // Insert into Flat index with timing
    std::cout << "Inserting into Flat index...\n";
    auto flat_start = std::chrono::high_resolution_clock::now();
    for (uint64_t id = 1; id <= num_vectors; ++id) {
        lynx::VectorRecord record;
        record.id = id;
        record.vector = all_vectors[id - 1];
        flat_db->insert(record);
    }
    auto flat_end = std::chrono::high_resolution_clock::now();
    auto flat_insertion_time = std::chrono::duration_cast<std::chrono::milliseconds>(flat_end - flat_start).count();

    // Insert into HNSW index with timing
    std::cout << "Inserting into HNSW index...\n";
    auto hnsw_start = std::chrono::high_resolution_clock::now();
    for (uint64_t id = 1; id <= num_vectors; ++id) {
        lynx::VectorRecord record;
        record.id = id;
        record.vector = all_vectors[id - 1];
        hnsw_db->insert(record);
    }
    auto hnsw_end = std::chrono::high_resolution_clock::now();
    auto hnsw_insertion_time = std::chrono::duration_cast<std::chrono::milliseconds>(hnsw_end - hnsw_start).count();

    // Insert into IVF index with timing (using batch_insert with max 100 entries per batch)
    std::cout << "Inserting into IVF index (batch mode)...\n";
    auto ivf_start = std::chrono::high_resolution_clock::now();
    const size_t batch_size = 100;
    std::vector<lynx::VectorRecord> ivf_records;
    ivf_records.reserve(batch_size);
    for (uint64_t id = 1; id <= num_vectors; ++id) {
        lynx::VectorRecord record;
        record.id = id;
        record.vector = all_vectors[id - 1];
        ivf_records.push_back(record);

        // Insert batch when it reaches batch_size or at the end
        if (ivf_records.size() == batch_size || id == num_vectors) {
            ivf_db->batch_insert(ivf_records);
            ivf_records.clear();
        }
    }
    auto ivf_end = std::chrono::high_resolution_clock::now();
    auto ivf_insertion_time = std::chrono::duration_cast<std::chrono::milliseconds>(ivf_end - ivf_start).count();

    std::cout << "\nInsertion Times:\n";
    std::cout << "  Flat: " << flat_insertion_time << " ms\n";
    std::cout << "  HNSW: " << hnsw_insertion_time << " ms";
    std::cout << " (" << std::setprecision(2)
              << (static_cast<double>(hnsw_insertion_time) / std::max(flat_insertion_time, 1L)) << "x)\n";
    std::cout << "  IVF:  " << ivf_insertion_time << " ms";
    std::cout << " (" << std::setprecision(2)
              << (static_cast<double>(ivf_insertion_time) / std::max(flat_insertion_time, 1L)) << "x)\n\n";

    std::cout << "Flat DB size: " << flat_db->size() << "\n";
    std::cout << "HNSW DB size: " << hnsw_db->size() << "\n";
    std::cout << "IVF DB size:  " << ivf_db->size() << "\n\n";

    // Run multiple queries
    const size_t num_queries = 5;
    std::vector<int> match_counts(k + 1, 0);  // Count matches for each position

    for (size_t q = 0; q < num_queries; ++q) {

        auto query = generate_random_vector(dimension, gen);

        auto flat_result = flat_db->search(query, k);
        auto hnsw_result = hnsw_db->search(query, k);
        auto ivf_result = ivf_db->search(query, k);

        std::string title = "Query " + std::to_string(q + 1) + " of " + std::to_string(num_queries);
        std::cout << title << "\n";
        compare_search_results(title, flat_result, hnsw_result, ivf_result, k);

        // Count position-wise matches
        for (size_t i = 0; i < k && i < flat_result.items.size() && i < hnsw_result.items.size(); ++i) {
            if (flat_result.items[i].id == hnsw_result.items[i].id) {
                match_counts[i]++;
            }
        }
    }

    // Summary statistics
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "SUMMARY STATISTICS (over " << num_queries << " queries)\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << "Position-wise match rate:\n";
    for (size_t i = 0; i < k; ++i) {
        std::cout << "  Rank " << std::setw(2) << (i + 1) << ": "
                  << std::setw(3) << (100 * match_counts[i] / num_queries) << "%\n";
    }

    std::cout << "\nConclusion:\n";
    std::cout << "Both HNSW and IVF are **approximate** nearest neighbor algorithms.\n";
    std::cout << "They trade perfect accuracy for speed and scalability.\n";
    std::cout << "Differences in results compared to Flat (brute-force) are expected.\n";
    std::cout << "HNSW typically offers higher recall, while IVF can be faster on large datasets.\n";

    return 0;
}
