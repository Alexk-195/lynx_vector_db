/**
 * @file compare_indices.cpp
 * @brief Compare search results between Flat and HNSW indices
 */

#include "lynx/lynx.h"
#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <set>
#include <chrono>

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
                            size_t k) {
    std::cout << "\n" << title << "\n";
    std::cout << std::string(60, '=') << "\n";

    // Check if all results are the same
    bool all_match = true;
    size_t matched = 0;

    std::set<uint64_t> flat_ids;
    for (const auto& item : flat_result.items) {
        flat_ids.insert(item.id);
    }

    std::set<uint64_t> hnsw_ids;
    for (const auto& item : hnsw_result.items) {
        hnsw_ids.insert(item.id);
    }

    // Count how many IDs match
    for (auto id : hnsw_ids) {
        if (flat_ids.find(id) != flat_ids.end()) {
            matched++;
        }
    }

    std::cout << "Matched IDs: " << matched << " / " << k << " ("
              << std::fixed << std::setprecision(1)
              << (100.0 * matched / k) << "%)\n";

    // Compare top results side-by-side
    std::cout << "\nRank | Flat Index              | HNSW Index              | Match\n";
    std::cout << "-----+-------------------------+-------------------------+------\n";

    for (size_t i = 0; i < k; ++i) {
        std::cout << std::setw(4) << (i + 1) << " | ";

        if (i < flat_result.items.size()) {
            std::cout << "ID=" << std::setw(5) << flat_result.items[i].id
                      << " D=" << std::setw(8) << std::setprecision(6)
                      << flat_result.items[i].distance;
        } else {
            std::cout << std::setw(21) << "-";
        }

        std::cout << " | ";

        if (i < hnsw_result.items.size()) {
            std::cout << "ID=" << std::setw(5) << hnsw_result.items[i].id
                      << " D=" << std::setw(8) << std::setprecision(6)
                      << hnsw_result.items[i].distance;
        } else {
            std::cout << std::setw(21) << "-";
        }

        std::cout << " | ";

        // Check if they match
        if (i < flat_result.items.size() && i < hnsw_result.items.size()) {
            if (flat_result.items[i].id == hnsw_result.items[i].id) {
                std::cout << " ✓";
                if (std::abs(flat_result.items[i].distance - hnsw_result.items[i].distance) < 1e-5) {
                    // Exact match
                } else {
                    all_match = false;
                }
            } else {
                std::cout << " ✗";
                all_match = false;
            }
        }

        std::cout << "\n";
    }

    // Check for missing results (in Flat but not in HNSW)
    std::cout << "\nMissing from HNSW (present in Flat):\n";
    bool found_missing = false;
    for (const auto& item : flat_result.items) {
        if (hnsw_ids.find(item.id) == hnsw_ids.end()) {
            std::cout << "  ID=" << item.id << " (Distance="
                      << std::setprecision(6) << item.distance << ")\n";
            found_missing = true;
        }
    }
    if (!found_missing) {
        std::cout << "  None\n";
    }

    // Check for extra results (in HNSW but not in Flat)
    std::cout << "\nExtra in HNSW (not in Flat top-k):\n";
    bool found_extra = false;
    for (const auto& item : hnsw_result.items) {
        if (flat_ids.find(item.id) == flat_ids.end()) {
            std::cout << "  ID=" << item.id << " (Distance="
                      << std::setprecision(6) << item.distance << ")\n";
            found_extra = true;
        }
    }
    if (!found_extra) {
        std::cout << "  None\n";
    }

    std::cout << "\nQuery Time:\n";
    std::cout << "  Flat: " << std::setprecision(3) << flat_result.query_time_ms << " ms\n";
    std::cout << "  HNSW: " << std::setprecision(3) << hnsw_result.query_time_ms << " ms\n";
    std::cout << "  Speedup: " << std::setprecision(2)
              << (flat_result.query_time_ms / std::max(hnsw_result.query_time_ms, 0.001))
              << "x\n";
}

int main() {
    std::cout << "================================================================\n";
    std::cout << "Lynx Vector Database - Flat vs HNSW Comparison\n";
    std::cout << "================================================================\n\n";

    // Fixed seed for reproducibility
    std::mt19937 gen(42);

    const size_t dimension = 128;
    const size_t num_vectors = 1000;
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

    std::cout << "Setup:\n";
    std::cout << "  Dimension: " << dimension << "\n";
    std::cout << "  Num vectors: " << num_vectors << "\n";
    std::cout << "  k (neighbors): " << k << "\n";
    std::cout << "  Distance metric: L2\n";
    std::cout << "  HNSW parameters: M=" << hnsw_config.hnsw_params.m
              << ", ef_construction=" << hnsw_config.hnsw_params.ef_construction
              << ", ef_search=" << hnsw_config.hnsw_params.ef_search << "\n\n";

    // Create databases
    auto flat_db = lynx::IVectorDatabase::create(flat_config);
    auto hnsw_db = lynx::IVectorDatabase::create(hnsw_config);

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

    std::cout << "\nInsertion Times:\n";
    std::cout << "  Flat: " << flat_insertion_time << " ms\n";
    std::cout << "  HNSW: " << hnsw_insertion_time << " ms\n";
    std::cout << "  Ratio (HNSW/Flat): " << std::setprecision(2)
              << (static_cast<double>(hnsw_insertion_time) / std::max(flat_insertion_time, 1L)) << "x\n\n";

    std::cout << "Flat DB size: " << flat_db->size() << "\n";
    std::cout << "HNSW DB size: " << hnsw_db->size() << "\n\n";

    // Run multiple queries
    const size_t num_queries = 5;
    std::vector<int> match_counts(k + 1, 0);  // Count matches for each position

    for (size_t q = 0; q < num_queries; ++q) {
        auto query = generate_random_vector(dimension, gen);

        auto flat_result = flat_db->search(query, k);
        auto hnsw_result = hnsw_db->search(query, k);

        std::string title = "Query " + std::to_string(q + 1) + " of " + std::to_string(num_queries);
        compare_search_results(title, flat_result, hnsw_result, k);

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
    std::cout << "HNSW is an **approximate** nearest neighbor algorithm.\n";
    std::cout << "It trades perfect accuracy for speed and scalability.\n";
    std::cout << "Differences in results are expected and by design.\n";

    return 0;
}
