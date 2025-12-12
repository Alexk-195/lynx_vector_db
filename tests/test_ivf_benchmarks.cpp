/**
 * @file test_ivf_benchmarks.cpp
 * @brief Performance benchmarks comparing IVF, HNSW, and Flat indices
 *
 * This file contains basic performance benchmarks to evaluate the IVF implementation
 * against other index types. For ticket #2005.
 *
 * @copyright MIT License
 */

#include "../src/lib/ivf_index.h"
#include "../src/lib/hnsw_index.h"
#include "../src/lib/flat_index.h"
#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include <iomanip>

using namespace lynx;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generate random vectors for benchmarking
 */
std::vector<std::vector<float>> generate_random_vectors(
    std::size_t count, std::size_t dimension, std::uint64_t seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<std::vector<float>> vectors;
    vectors.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        std::vector<float> vec(dimension);
        for (std::size_t d = 0; d < dimension; ++d) {
            vec[d] = dist(rng);
        }
        vectors.push_back(std::move(vec));
    }

    return vectors;
}

/**
 * @brief Calculate recall@k between two result sets
 */
double calculate_recall(
    const std::vector<SearchResultItem>& ground_truth,
    const std::vector<SearchResultItem>& results,
    std::size_t k) {

    if (ground_truth.empty() || results.empty()) {
        return 0.0;
    }

    std::size_t matches = 0;
    std::size_t check_size = std::min(k, std::min(ground_truth.size(), results.size()));

    for (std::size_t i = 0; i < check_size; ++i) {
        for (std::size_t j = 0; j < check_size; ++j) {
            if (results[j].id == ground_truth[i].id) {
                matches++;
                break;
            }
        }
    }

    return static_cast<double>(matches) / static_cast<double>(check_size);
}

// ============================================================================
// Construction Time Benchmarks
// ============================================================================

TEST(IVFBenchmark, ConstructionTime_10K_Dim128) {
    const std::size_t num_vectors = 10000;
    const std::size_t dimension = 128;

    auto vectors = generate_random_vectors(num_vectors, dimension);

    // Prepare VectorRecords
    std::vector<VectorRecord> records;
    records.reserve(num_vectors);
    for (std::size_t i = 0; i < num_vectors; ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    std::cout << "\n=== Construction Time Benchmark (10K vectors, dim=128) ===" << std::endl;

    // IVF construction
    {
        IVFParams ivf_params;
        ivf_params.n_clusters = 100;  // sqrt(10K) ≈ 100
        ivf_params.n_probe = 10;

        IVFIndex index(dimension, DistanceMetric::L2, ivf_params);

        auto start = std::chrono::high_resolution_clock::now();
        EXPECT_EQ(index.build(records), ErrorCode::Ok);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "IVF:  " << duration.count() << " ms" << std::endl;
        std::cout << "      Memory: " << index.memory_usage() / 1024 << " KB" << std::endl;
    }

    // HNSW construction (for comparison)
    {
        HNSWParams hnsw_params;
        hnsw_params.M = 16;
        hnsw_params.ef_construction = 200;

        HNSWIndex index(dimension, DistanceMetric::L2, hnsw_params);

        auto start = std::chrono::high_resolution_clock::now();
        for (const auto& record : records) {
            index.add(record.id, record.vector);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "HNSW: " << duration.count() << " ms" << std::endl;
        std::cout << "      Memory: " << index.memory_usage() / 1024 << " KB" << std::endl;
    }
}

// ============================================================================
// Query Performance Benchmarks
// ============================================================================

TEST(IVFBenchmark, QueryLatency_10K_Dim128) {
    const std::size_t num_vectors = 10000;
    const std::size_t dimension = 128;
    const std::size_t num_queries = 100;
    const std::size_t k = 10;

    auto vectors = generate_random_vectors(num_vectors, dimension);
    auto queries = generate_random_vectors(num_queries, dimension, 999);

    // Prepare VectorRecords
    std::vector<VectorRecord> records;
    records.reserve(num_vectors);
    for (std::size_t i = 0; i < num_vectors; ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    std::cout << "\n=== Query Latency Benchmark (10K vectors, dim=128, k=10) ===" << std::endl;

    // IVF with different n_probe values
    {
        IVFParams ivf_params;
        ivf_params.n_clusters = 100;
        ivf_params.n_probe = 1;

        IVFIndex index(dimension, DistanceMetric::L2, ivf_params);
        index.build(records);

        for (std::size_t n_probe : {1, 5, 10, 20}) {
            SearchParams params;
            params.n_probe = n_probe;

            auto start = std::chrono::high_resolution_clock::now();
            for (const auto& query : queries) {
                auto results = index.search(query, k, params);
            }
            auto end = std::chrono::high_resolution_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double avg_latency = static_cast<double>(duration.count()) / num_queries / 1000.0;
            std::cout << "IVF (n_probe=" << std::setw(2) << n_probe << "): "
                      << std::fixed << std::setprecision(3) << avg_latency << " ms/query" << std::endl;
        }
    }

    // HNSW for comparison
    {
        HNSWParams hnsw_params;
        hnsw_params.M = 16;
        hnsw_params.ef_construction = 200;

        HNSWIndex index(dimension, DistanceMetric::L2, hnsw_params);
        for (const auto& record : records) {
            index.add(record.id, record.vector);
        }

        SearchParams params;
        params.ef_search = 50;

        auto start = std::chrono::high_resolution_clock::now();
        for (const auto& query : queries) {
            auto results = index.search(query, k, params);
        }
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_latency = static_cast<double>(duration.count()) / num_queries / 1000.0;
        std::cout << "HNSW (ef=50):       "
                  << std::fixed << std::setprecision(3) << avg_latency << " ms/query" << std::endl;
    }
}

// ============================================================================
// Recall Evaluation
// ============================================================================

TEST(IVFBenchmark, RecallVsNProbe_1K_Dim64) {
    const std::size_t num_vectors = 1000;
    const std::size_t dimension = 64;
    const std::size_t num_queries = 50;
    const std::size_t k = 10;

    auto vectors = generate_random_vectors(num_vectors, dimension);
    auto queries = generate_random_vectors(num_queries, dimension, 777);

    // Prepare VectorRecords
    std::vector<VectorRecord> records;
    records.reserve(num_vectors);
    for (std::size_t i = 0; i < num_vectors; ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    // Build ground truth with brute-force (Flat index)
    std::vector<std::vector<SearchResultItem>> ground_truth;
    {
        FlatIndex flat_index(dimension, DistanceMetric::L2);
        for (const auto& record : records) {
            flat_index.add(record.id, record.vector);
        }

        for (const auto& query : queries) {
            SearchParams params;
            auto results = flat_index.search(query, k, params);
            ground_truth.push_back(results);
        }
    }

    // Build IVF index
    IVFParams ivf_params;
    ivf_params.n_clusters = 32;  // sqrt(1000) ≈ 32
    ivf_params.n_probe = 1;

    IVFIndex index(dimension, DistanceMetric::L2, ivf_params);
    index.build(records);

    std::cout << "\n=== Recall@10 vs n_probe (1K vectors, dim=64) ===" << std::endl;

    // Test different n_probe values
    for (std::size_t n_probe : {1, 2, 4, 8, 16, 32}) {
        SearchParams params;
        params.n_probe = n_probe;

        double total_recall = 0.0;
        for (std::size_t q = 0; q < num_queries; ++q) {
            auto results = index.search(queries[q], k, params);
            double recall = calculate_recall(ground_truth[q], results, k);
            total_recall += recall;
        }

        double avg_recall = total_recall / num_queries;
        std::cout << "n_probe=" << std::setw(2) << n_probe << ": "
                  << std::fixed << std::setprecision(1) << (avg_recall * 100.0) << "% recall" << std::endl;
    }
}

// ============================================================================
// Scalability Tests
// ============================================================================

TEST(IVFBenchmark, ScalabilityAcrossDatasetSizes) {
    const std::size_t dimension = 128;
    const std::size_t k = 10;

    std::cout << "\n=== Scalability: Construction Time vs Dataset Size (dim=128) ===" << std::endl;

    for (std::size_t num_vectors : {1000, 5000, 10000, 50000}) {
        auto vectors = generate_random_vectors(num_vectors, dimension);

        std::vector<VectorRecord> records;
        records.reserve(num_vectors);
        for (std::size_t i = 0; i < num_vectors; ++i) {
            records.push_back({i, vectors[i], std::nullopt});
        }

        IVFParams ivf_params;
        ivf_params.n_clusters = static_cast<std::size_t>(std::sqrt(num_vectors));
        ivf_params.n_probe = 10;

        IVFIndex index(dimension, DistanceMetric::L2, ivf_params);

        auto start = std::chrono::high_resolution_clock::now();
        index.build(records);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << std::setw(6) << num_vectors << " vectors: "
                  << std::setw(6) << duration.count() << " ms"
                  << "  (memory: " << index.memory_usage() / 1024 << " KB)" << std::endl;
    }
}

// ============================================================================
// Memory Usage Comparison
// ============================================================================

TEST(IVFBenchmark, MemoryUsageComparison_10K_Dim128) {
    const std::size_t num_vectors = 10000;
    const std::size_t dimension = 128;

    auto vectors = generate_random_vectors(num_vectors, dimension);

    std::vector<VectorRecord> records;
    records.reserve(num_vectors);
    for (std::size_t i = 0; i < num_vectors; ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    std::cout << "\n=== Memory Usage Comparison (10K vectors, dim=128) ===" << std::endl;

    // Flat
    {
        FlatIndex index(dimension, DistanceMetric::L2);
        for (const auto& record : records) {
            index.add(record.id, record.vector);
        }
        std::cout << "Flat: " << std::setw(6) << index.memory_usage() / 1024 << " KB" << std::endl;
    }

    // IVF
    {
        IVFParams ivf_params;
        ivf_params.n_clusters = 100;
        ivf_params.n_probe = 10;

        IVFIndex index(dimension, DistanceMetric::L2, ivf_params);
        index.build(records);
        std::cout << "IVF:  " << std::setw(6) << index.memory_usage() / 1024 << " KB" << std::endl;
    }

    // HNSW
    {
        HNSWParams hnsw_params;
        hnsw_params.M = 16;
        hnsw_params.ef_construction = 200;

        HNSWIndex index(dimension, DistanceMetric::L2, hnsw_params);
        for (const auto& record : records) {
            index.add(record.id, record.vector);
        }
        std::cout << "HNSW: " << std::setw(6) << index.memory_usage() / 1024 << " KB" << std::endl;
    }
}
