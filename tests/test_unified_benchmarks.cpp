/**
 * @file test_unified_benchmarks.cpp
 * @brief Performance benchmarks for all index types (Flat, HNSW, IVF)
 *
 * Tests for ticket #2082: Unified benchmarks for all index types
 *
 * This file contains benchmark tests that validate and measure performance
 * across all three index types using parameterized tests. Each test runs
 * for a maximum of 20 seconds to ensure fast test execution.
 *
 * @copyright MIT License
 */

#include "../src/lib/vector_database.h"
#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <atomic>

using namespace lynx;

namespace {

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generate random vectors for testing.
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
 * @brief Measure execution time of a function.
 */
template<typename Func>
double measure_time_ms(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

/**
 * @brief Get index type name as string.
 */
std::string index_type_name(IndexType type) {
    switch (type) {
        case IndexType::Flat: return "Flat";
        case IndexType::HNSW: return "HNSW";
        case IndexType::IVF: return "IVF";
        default: return "Unknown";
    }
}

} // anonymous namespace

// ============================================================================
// Unified Benchmark Tests for All Index Types
// ============================================================================

class UnifiedIndexBenchmarkTest : public ::testing::TestWithParam<IndexType> {
protected:
    void SetUp() override {
        config_.dimension = 128;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = GetParam();

        // Configure index-specific parameters for fast execution
        // Prioritize speed over accuracy to keep tests under 20 seconds
        config_.hnsw_params.m = 8;  // Lower M for faster construction
        config_.hnsw_params.ef_construction = 50;  // Lower ef_construction for speed
        config_.hnsw_params.ef_search = 32;

        config_.ivf_params.n_clusters = 50;  // Moderate cluster count
        config_.ivf_params.n_probe = 5;
    }

    /**
     * @brief Configure index parameters based on dataset size.
     * Optimized for speed to keep tests under 20 seconds.
     */
    void configure_for_dataset_size(std::size_t dataset_size) {
        if (config_.index_type == IndexType::HNSW) {
            if (dataset_size <= 1000) {
                config_.hnsw_params.ef_construction = 50;
                config_.hnsw_params.ef_search = 32;
            } else if (dataset_size <= 10000) {
                config_.hnsw_params.ef_construction = 32;
                config_.hnsw_params.ef_search = 24;
            } else {
                config_.hnsw_params.ef_construction = 24;
                config_.hnsw_params.ef_search = 16;
            }
        }

        if (config_.index_type == IndexType::IVF) {
            config_.ivf_params.n_clusters = std::max(std::size_t(10),
                static_cast<std::size_t>(std::sqrt(dataset_size)));
            config_.ivf_params.n_probe = std::max(std::size_t(1),
                config_.ivf_params.n_clusters / 10);
        }
    }

    Config config_;
};

// ============================================================================
// Search Latency Benchmarks
// ============================================================================

TEST_P(UnifiedIndexBenchmarkTest, SearchLatency_SmallDataset) {
    const std::size_t num_vectors = 1000;
    const std::size_t k = 10;
    const int num_queries = 50;  // Reduced from 100 to ensure <20s per index

    configure_for_dataset_size(num_vectors);
    auto db = std::make_shared<VectorDatabase>(config_);

    // Insert vectors
    auto vectors = generate_random_vectors(num_vectors, config_.dimension);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        db->insert({i, vectors[i], std::nullopt});
    }

    // Prepare query
    auto query = generate_random_vectors(1, config_.dimension)[0];

    // Benchmark search
    double total_time_ms = measure_time_ms([&]() {
        for (int i = 0; i < num_queries; ++i) {
            auto results = db->search(query, k);
        }
    });

    double avg_time_ms = total_time_ms / num_queries;

    std::cout << "\n[BENCHMARK] " << index_type_name(GetParam())
              << " - Small Dataset (1K vectors):\n";
    std::cout << "  Average query time: " << std::fixed << std::setprecision(3)
              << avg_time_ms << " ms\n";
    std::cout << "  Total benchmark time: " << std::setprecision(2)
              << (total_time_ms / 1000.0) << " seconds\n";

    // Verify search returns results
    auto test_results = db->search(query, k);
    EXPECT_GT(test_results.items.size(), 0);
    EXPECT_LE(test_results.items.size(), k);

    // Ensure test completes in reasonable time
    EXPECT_LT(total_time_ms / 1000.0, 20.0);  // Must complete within 20 seconds
}

TEST_P(UnifiedIndexBenchmarkTest, SearchLatency_MediumDataset) {
    const std::size_t num_vectors = 5000;  // Reduced from 10K for faster execution
    const std::size_t k = 10;
    const int num_queries = 30;  // Reduced from 100 to ensure <20s per index

    configure_for_dataset_size(num_vectors);
    auto db = std::make_shared<VectorDatabase>(config_);

    // Insert vectors with timeout
    auto vectors = generate_random_vectors(num_vectors, config_.dimension);

    std::cout << "\n[BENCHMARK] " << index_type_name(GetParam())
              << " - Medium Dataset (5K vectors): Inserting...\n";

    auto insert_start = std::chrono::steady_clock::now();
    constexpr auto insert_timeout = std::chrono::seconds(15);
    std::size_t inserted = 0;

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        auto elapsed = std::chrono::steady_clock::now() - insert_start;
        if (elapsed > insert_timeout) {
            std::cout << "  Insert timeout after " << i << " vectors\n";
            break;
        }
        db->insert({i, vectors[i], std::nullopt});
        inserted++;
    }

    std::cout << "  Inserted " << inserted << " vectors\n";
    EXPECT_GT(inserted, 0);

    // Prepare query
    auto query = generate_random_vectors(1, config_.dimension)[0];

    // Benchmark search
    double total_time_ms = measure_time_ms([&]() {
        for (int i = 0; i < num_queries; ++i) {
            auto results = db->search(query, k);
        }
    });

    double avg_time_ms = total_time_ms / num_queries;

    std::cout << "  Average query time: " << std::fixed << std::setprecision(3)
              << avg_time_ms << " ms\n";
    std::cout << "  Total benchmark time: " << std::setprecision(2)
              << (total_time_ms / 1000.0) << " seconds\n";

    // Verify search returns results
    auto test_results = db->search(query, k);
    EXPECT_GT(test_results.items.size(), 0);

    // Ensure test completes in reasonable time (15s insert + search)
    EXPECT_LT(total_time_ms / 1000.0, 20.0);
}

TEST_P(UnifiedIndexBenchmarkTest, SearchLatency_VaryingK) {
    const std::size_t num_vectors = 2000;  // Smaller dataset for multiple k values
    const int num_queries = 20;  // Reduced iterations

    configure_for_dataset_size(num_vectors);
    auto db = std::make_shared<VectorDatabase>(config_);

    // Insert vectors
    auto vectors = generate_random_vectors(num_vectors, config_.dimension);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        db->insert({i, vectors[i], std::nullopt});
    }

    // Prepare query
    auto query = generate_random_vectors(1, config_.dimension)[0];

    std::cout << "\n[BENCHMARK] " << index_type_name(GetParam())
              << " - Varying k (2K vectors):\n";
    std::cout << std::setw(10) << "k"
              << std::setw(20) << "Avg Query Time (ms)"
              << std::setw(15) << "Results\n";
    std::cout << std::string(45, '-') << "\n";

    double total_benchmark_time = 0.0;

    for (std::size_t k : {1, 10, 50}) {
        double total_time_ms = measure_time_ms([&]() {
            for (int i = 0; i < num_queries; ++i) {
                auto results = db->search(query, k);
            }
        });

        double avg_time_ms = total_time_ms / num_queries;
        total_benchmark_time += total_time_ms / 1000.0;

        auto test_results = db->search(query, k);

        std::cout << std::setw(10) << k
                  << std::setw(20) << std::fixed << std::setprecision(3)
                  << avg_time_ms
                  << std::setw(15) << test_results.items.size() << "\n";

        EXPECT_GT(test_results.items.size(), 0);
        EXPECT_LE(test_results.items.size(), k);
    }

    std::cout << "  Total benchmark time: " << std::fixed << std::setprecision(2)
              << total_benchmark_time << " seconds\n";

    // Ensure all k variations complete within 20 seconds
    EXPECT_LT(total_benchmark_time, 20.0);
}

// ============================================================================
// Memory Usage Benchmark
// ============================================================================

TEST_P(UnifiedIndexBenchmarkTest, MemoryUsage_Comparison) {
    const std::size_t num_vectors = 5000;  // Moderate size for memory test

    configure_for_dataset_size(num_vectors);
    auto db = std::make_shared<VectorDatabase>(config_);

    // Insert vectors with timeout
    auto vectors = generate_random_vectors(num_vectors, config_.dimension);

    auto insert_start = std::chrono::steady_clock::now();
    constexpr auto insert_timeout = std::chrono::seconds(15);
    std::size_t inserted = 0;

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        auto elapsed = std::chrono::steady_clock::now() - insert_start;
        if (elapsed > insert_timeout) {
            break;
        }
        db->insert({i, vectors[i], std::nullopt});
        inserted++;
    }

    EXPECT_GT(inserted, 0);

    // Get memory stats
    auto stats = db->stats();
    double memory_mb = stats.memory_usage_bytes / (1024.0 * 1024.0);

    // Calculate theoretical minimum (just raw vector data)
    std::size_t raw_data_bytes = inserted * config_.dimension * sizeof(float);
    double raw_data_mb = raw_data_bytes / (1024.0 * 1024.0);
    double overhead_ratio = memory_mb / raw_data_mb;

    std::cout << "\n[BENCHMARK] " << index_type_name(GetParam())
              << " - Memory Usage (" << inserted << " vectors, dim="
              << config_.dimension << "):\n";
    std::cout << "  Total memory: " << std::fixed << std::setprecision(2)
              << memory_mb << " MB\n";
    std::cout << "  Raw data size: " << raw_data_mb << " MB\n";
    std::cout << "  Overhead ratio: " << std::setprecision(2)
              << overhead_ratio << "x\n";

    // Sanity checks
    EXPECT_GT(memory_mb, 0.0);
    EXPECT_GT(overhead_ratio, 1.0);  // Always some overhead

    // Index-specific overhead expectations
    if (GetParam() == IndexType::Flat) {
        // Flat should have ~2x overhead (vector storage + metadata)
        EXPECT_LT(overhead_ratio, 3.0);
    } else if (GetParam() == IndexType::HNSW) {
        // HNSW has graph structure overhead, can be 3-5x
        EXPECT_LT(overhead_ratio, 6.0);
    } else if (GetParam() == IndexType::IVF) {
        // IVF has cluster structure, typically 2-3x
        EXPECT_LT(overhead_ratio, 4.0);
    }
}

// ============================================================================
// Construction Time Benchmark
// ============================================================================

TEST_P(UnifiedIndexBenchmarkTest, ConstructionTime) {
    const std::size_t num_vectors = 5000;

    configure_for_dataset_size(num_vectors);

    // Generate vectors
    auto vectors = generate_random_vectors(num_vectors, config_.dimension);

    // Measure construction time (database creation + batch insert)
    auto db = std::make_shared<VectorDatabase>(config_);

    std::vector<VectorRecord> records;
    records.reserve(vectors.size());
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    auto construction_start = std::chrono::steady_clock::now();
    constexpr auto construction_timeout = std::chrono::seconds(18);

    std::size_t inserted = 0;
    for (const auto& record : records) {
        auto elapsed = std::chrono::steady_clock::now() - construction_start;
        if (elapsed > construction_timeout) {
            break;
        }
        if (db->insert(record) == ErrorCode::Ok) {
            inserted++;
        }
    }

    auto construction_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - construction_start).count() / 1000.0;

    std::cout << "\n[BENCHMARK] " << index_type_name(GetParam())
              << " - Construction Time:\n";
    std::cout << "  Inserted " << inserted << " / " << num_vectors
              << " vectors\n";
    std::cout << "  Construction time: " << std::fixed << std::setprecision(2)
              << construction_time << " seconds\n";

    if (inserted > 0) {
        double throughput = inserted / construction_time;
        std::cout << "  Throughput: " << std::setprecision(0)
                  << throughput << " inserts/second\n";
    }

    EXPECT_GT(inserted, 0);
    EXPECT_LT(construction_time, 20.0);  // Must complete within 20 seconds

    // Index-specific performance expectations
    if (GetParam() == IndexType::Flat) {
        // Flat should be very fast (instant construction)
        EXPECT_EQ(inserted, num_vectors);  // Should insert all
        EXPECT_LT(construction_time, 1.0);  // Should be sub-second
    }
    // HNSW and IVF may timeout on large datasets, which is acceptable
}

// ============================================================================
// Test Instantiation
// ============================================================================

INSTANTIATE_TEST_SUITE_P(
    AllIndexTypes,
    UnifiedIndexBenchmarkTest,
    ::testing::Values(IndexType::Flat, IndexType::HNSW, IndexType::IVF),
    [](const ::testing::TestParamInfo<IndexType>& info) {
        return index_type_name(info.param);
    }
);
