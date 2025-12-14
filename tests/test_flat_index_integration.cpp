/**
 * @file test_flat_index_integration.cpp
 * @brief Integration tests for FlatIndex
 *
 * Tests for ticket #2053: Test FlatIndex Integration
 *
 * This file contains integration tests that validate FlatIndex provides
 * feature parity with VectorDatabase_Impl flat search implementation,
 * along with performance benchmarks and end-to-end testing.
 *
 * @copyright MIT License
 */

#include "../src/lib/flat_index.h"
#include "../src/lib/vector_database_flat.h"
#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <iomanip>

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
 * @brief Generate normalized vectors for cosine similarity testing.
 */
std::vector<std::vector<float>> generate_normalized_vectors(
    std::size_t count, std::size_t dimension, std::uint64_t seed = 42) {
    auto vectors = generate_random_vectors(count, dimension, seed);

    // Normalize each vector
    for (auto& vec : vectors) {
        float norm = 0.0f;
        for (float v : vec) {
            norm += v * v;
        }
        norm = std::sqrt(norm);
        if (norm > 0.0f) {
            for (float& v : vec) {
                v /= norm;
            }
        }
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
 * @brief Compare two search results for equality.
 */
bool results_equal(const std::vector<SearchResultItem>& a,
                   const std::vector<SearchResultItem>& b,
                   float distance_tolerance = 1e-5f) {
    if (a.size() != b.size()) {
        return false;
    }

    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id) {
            return false;
        }
        if (std::abs(a[i].distance - b[i].distance) > distance_tolerance) {
            return false;
        }
    }

    return true;
}

} // anonymous namespace

// ============================================================================
// Integration Tests: FlatIndex vs VectorDatabase_Impl
// ============================================================================

class FlatIndexIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default configuration
        config_.dimension = 128;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = IndexType::Flat;
    }

    Config config_;
};

TEST_F(FlatIndexIntegrationTest, IdenticalSearchResults_EmptyIndex) {
    // Create both implementations
    FlatIndex flat_index(config_.dimension, config_.distance_metric);
    auto db = IVectorDatabase::create(config_);

    // Query empty indices
    std::vector<float> query(config_.dimension, 0.5f);
    auto flat_results = flat_index.search(query, 10, SearchParams{});
    auto db_results = db->search(query, 10);

    // Both should return empty results
    EXPECT_TRUE(flat_results.empty());
    EXPECT_TRUE(db_results.items.empty());
}

TEST_F(FlatIndexIntegrationTest, IdenticalSearchResults_SingleVector) {
    FlatIndex flat_index(config_.dimension, config_.distance_metric);
    auto db = IVectorDatabase::create(config_);

    // Insert same vector into both
    std::vector<float> vec(config_.dimension, 1.0f);
    flat_index.add(1, vec);
    db->insert({1, vec, std::nullopt});

    // Query both
    std::vector<float> query(config_.dimension, 0.5f);
    auto flat_results = flat_index.search(query, 10, SearchParams{});
    auto db_results = db->search(query, 10);

    // Results should be identical
    ASSERT_EQ(flat_results.size(), 1);
    ASSERT_EQ(db_results.items.size(), 1);
    EXPECT_EQ(flat_results[0].id, db_results.items[0].id);
    EXPECT_NEAR(flat_results[0].distance, db_results.items[0].distance, 1e-5f);
}

TEST_F(FlatIndexIntegrationTest, IdenticalSearchResults_MultipleVectors) {
    FlatIndex flat_index(config_.dimension, config_.distance_metric);
    auto db = IVectorDatabase::create(config_);

    // Generate and insert vectors
    auto vectors = generate_random_vectors(100, config_.dimension);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        flat_index.add(i, vectors[i]);
        db->insert({i, vectors[i], std::nullopt});
    }

    // Perform multiple queries
    auto queries = generate_random_vectors(10, config_.dimension, 123);
    for (const auto& query : queries) {
        auto flat_results = flat_index.search(query, 10, SearchParams{});
        auto db_results = db->search(query, 10);

        ASSERT_EQ(flat_results.size(), db_results.items.size());
        EXPECT_TRUE(results_equal(flat_results, db_results.items));
    }
}

TEST_F(FlatIndexIntegrationTest, IdenticalSearchResults_AllMetrics) {
    const std::size_t num_vectors = 50;
    const std::size_t k = 10;

    // Test each distance metric
    for (auto metric : {DistanceMetric::L2, DistanceMetric::Cosine, DistanceMetric::DotProduct}) {
        FlatIndex flat_index(config_.dimension, metric);

        Config db_config = config_;
        db_config.distance_metric = metric;
        auto db = IVectorDatabase::create(db_config);

        // Use normalized vectors for Cosine
        auto vectors = (metric == DistanceMetric::Cosine)
            ? generate_normalized_vectors(num_vectors, config_.dimension)
            : generate_random_vectors(num_vectors, config_.dimension);

        // Insert vectors
        for (std::size_t i = 0; i < vectors.size(); ++i) {
            flat_index.add(i, vectors[i]);
            db->insert({i, vectors[i], std::nullopt});
        }

        // Query
        std::vector<float> query = (metric == DistanceMetric::Cosine)
            ? generate_normalized_vectors(1, config_.dimension)[0]
            : generate_random_vectors(1, config_.dimension)[0];

        auto flat_results = flat_index.search(query, k, SearchParams{});
        auto db_results = db->search(query, k);

        ASSERT_EQ(flat_results.size(), db_results.items.size());
        EXPECT_TRUE(results_equal(flat_results, db_results.items, 1e-4f));
    }
}

TEST_F(FlatIndexIntegrationTest, IdenticalSearchResults_WithFiltering) {
    FlatIndex flat_index(config_.dimension, config_.distance_metric);
    auto db = IVectorDatabase::create(config_);

    // Insert vectors
    auto vectors = generate_random_vectors(100, config_.dimension);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        flat_index.add(i, vectors[i]);
        db->insert({i, vectors[i], std::nullopt});
    }

    // Query with filter (only even IDs)
    SearchParams params;
    params.filter = [](std::uint64_t id) { return id % 2 == 0; };

    std::vector<float> query(config_.dimension, 0.5f);
    auto flat_results = flat_index.search(query, 10, params);
    auto db_results = db->search(query, 10, params);

    ASSERT_EQ(flat_results.size(), db_results.items.size());
    EXPECT_TRUE(results_equal(flat_results, db_results.items));

    // Verify all results are filtered
    for (const auto& item : flat_results) {
        EXPECT_EQ(item.id % 2, 0);
    }
}

TEST_F(FlatIndexIntegrationTest, IdenticalBehavior_EdgeCases) {
    FlatIndex flat_index(config_.dimension, config_.distance_metric);
    auto db = IVectorDatabase::create(config_);

    auto vectors = generate_random_vectors(10, config_.dimension);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        flat_index.add(i, vectors[i]);
        db->insert({i, vectors[i], std::nullopt});
    }

    std::vector<float> query(config_.dimension, 0.5f);

    // Test k=0
    {
        auto flat_results = flat_index.search(query, 0, SearchParams{});
        auto db_results = db->search(query, 0);
        EXPECT_TRUE(flat_results.empty());
        EXPECT_TRUE(db_results.items.empty());
    }

    // Test k > size
    {
        auto flat_results = flat_index.search(query, 100, SearchParams{});
        auto db_results = db->search(query, 100);
        EXPECT_EQ(flat_results.size(), 10);
        EXPECT_EQ(db_results.items.size(), 10);
        EXPECT_TRUE(results_equal(flat_results, db_results.items));
    }
}

TEST_F(FlatIndexIntegrationTest, ConsistentErrorHandling_DimensionMismatch) {
    FlatIndex flat_index(config_.dimension, config_.distance_metric);
    auto db = IVectorDatabase::create(config_);

    // Try to add vector with wrong dimension
    std::vector<float> wrong_vec(config_.dimension / 2, 1.0f);

    auto flat_result = flat_index.add(1, wrong_vec);
    auto db_result = db->insert({1, wrong_vec, std::nullopt});

    EXPECT_EQ(flat_result, ErrorCode::DimensionMismatch);
    EXPECT_EQ(db_result, ErrorCode::DimensionMismatch);
    EXPECT_EQ(flat_index.size(), 0);
    EXPECT_EQ(db->size(), 0);
}

// ============================================================================
// Performance Benchmark Tests
// ============================================================================

class FlatIndexBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.dimension = 128;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = IndexType::Flat;
    }

    struct BenchmarkResult {
        double flat_index_time_ms;
        double database_time_ms;
        double speedup_ratio;
        std::size_t num_vectors;
        std::size_t k;
    };

    BenchmarkResult run_search_benchmark(std::size_t num_vectors, std::size_t k) {
        FlatIndex flat_index(config_.dimension, config_.distance_metric);
        auto db = IVectorDatabase::create(config_);

        // Prepare data
        auto vectors = generate_random_vectors(num_vectors, config_.dimension);
        for (std::size_t i = 0; i < vectors.size(); ++i) {
            flat_index.add(i, vectors[i]);
            db->insert({i, vectors[i], std::nullopt});
        }

        auto query = generate_random_vectors(1, config_.dimension)[0];

        // Benchmark FlatIndex
        double flat_time = measure_time_ms([&]() {
            for (int i = 0; i < 100; ++i) {
                auto results = flat_index.search(query, k, SearchParams{});
            }
        });

        // Benchmark VectorDatabase
        double db_time = measure_time_ms([&]() {
            for (int i = 0; i < 100; ++i) {
                auto results = db->search(query, k);
            }
        });

        return BenchmarkResult{
            flat_time / 100.0,  // Average per query
            db_time / 100.0,
            db_time / flat_time,
            num_vectors,
            k
        };
    }

    Config config_;
};

TEST_F(FlatIndexBenchmarkTest, SearchLatency_SmallDataset) {
    auto result = run_search_benchmark(1000, 10);

    std::cout << "\n[BENCHMARK] Small Dataset (1K vectors):\n";
    std::cout << "  FlatIndex:  " << std::fixed << std::setprecision(3)
              << result.flat_index_time_ms << " ms/query\n";
    std::cout << "  Database:   " << result.database_time_ms << " ms/query\n";
    std::cout << "  Speedup:    " << std::setprecision(2)
              << result.speedup_ratio << "x\n";

    // Performance should be within 5% of each other (allow for some variance)
    EXPECT_NEAR(result.speedup_ratio, 1.0, 0.15);
}

TEST_F(FlatIndexBenchmarkTest, SearchLatency_MediumDataset) {
    auto result = run_search_benchmark(10000, 10);

    std::cout << "\n[BENCHMARK] Medium Dataset (10K vectors):\n";
    std::cout << "  FlatIndex:  " << std::fixed << std::setprecision(3)
              << result.flat_index_time_ms << " ms/query\n";
    std::cout << "  Database:   " << result.database_time_ms << " ms/query\n";
    std::cout << "  Speedup:    " << std::setprecision(2)
              << result.speedup_ratio << "x\n";

    EXPECT_NEAR(result.speedup_ratio, 1.0, 0.15);
}

TEST_F(FlatIndexBenchmarkTest, SearchLatency_VaryingK) {
    const std::size_t num_vectors = 5000;

    std::cout << "\n[BENCHMARK] Varying k (5K vectors):\n";
    std::cout << std::setw(10) << "k"
              << std::setw(15) << "FlatIndex(ms)"
              << std::setw(15) << "Database(ms)"
              << std::setw(12) << "Speedup\n";
    std::cout << std::string(52, '-') << "\n";

    for (std::size_t k : {1, 10, 100}) {
        auto result = run_search_benchmark(num_vectors, k);

        std::cout << std::setw(10) << k
                  << std::setw(15) << std::fixed << std::setprecision(3)
                  << result.flat_index_time_ms
                  << std::setw(15) << result.database_time_ms
                  << std::setw(12) << std::setprecision(2)
                  << result.speedup_ratio << "x\n";

        EXPECT_NEAR(result.speedup_ratio, 1.0, 0.20);
    }
}

TEST_F(FlatIndexBenchmarkTest, MemoryUsage_Comparison) {
    const std::size_t num_vectors = 10000;

    FlatIndex flat_index(config_.dimension, config_.distance_metric);
    auto db = IVectorDatabase::create(config_);

    auto vectors = generate_random_vectors(num_vectors, config_.dimension);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        flat_index.add(i, vectors[i]);
        db->insert({i, vectors[i], std::nullopt});
    }

    std::size_t flat_memory = flat_index.memory_usage();
    std::size_t db_memory = db->stats().memory_usage_bytes;

    std::cout << "\n[BENCHMARK] Memory Usage (10K vectors, dim=128):\n";
    std::cout << "  FlatIndex:  " << (flat_memory / 1024.0 / 1024.0) << " MB\n";
    std::cout << "  Database:   " << (db_memory / 1024.0 / 1024.0) << " MB\n";

    // Memory should be similar (both store vectors + some overhead)
    double memory_ratio = static_cast<double>(db_memory) / flat_memory;
    EXPECT_NEAR(memory_ratio, 1.0, 0.3);  // Within 30%
}

// ============================================================================
// End-to-End Tests with Varying Dataset Sizes
// ============================================================================

class FlatIndexEndToEndTest : public ::testing::TestWithParam<std::size_t> {
protected:
    void SetUp() override {
        config_.dimension = 128;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = IndexType::Flat;
        dataset_size_ = GetParam();
    }

    Config config_;
    std::size_t dataset_size_;
};

TEST_P(FlatIndexEndToEndTest, InsertAndSearch) {
    FlatIndex flat_index(config_.dimension, config_.distance_metric);

    // Insert vectors
    auto vectors = generate_random_vectors(dataset_size_, config_.dimension);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        ASSERT_EQ(flat_index.add(i, vectors[i]), ErrorCode::Ok);
    }

    EXPECT_EQ(flat_index.size(), dataset_size_);

    // Search for different k values
    std::vector<float> query(config_.dimension, 0.5f);

    for (std::size_t k : {1, 10, 100}) {
        auto results = flat_index.search(query, k, SearchParams{});

        std::size_t expected_k = std::min(k, dataset_size_);
        EXPECT_EQ(results.size(), expected_k);

        // Verify results are sorted by distance
        for (std::size_t i = 1; i < results.size(); ++i) {
            EXPECT_LE(results[i-1].distance, results[i].distance);
        }
    }
}

TEST_P(FlatIndexEndToEndTest, BatchInsertAndSearch) {
    FlatIndex flat_index(config_.dimension, config_.distance_metric);

    // Prepare batch
    auto vectors = generate_random_vectors(dataset_size_, config_.dimension);
    std::vector<VectorRecord> records;
    records.reserve(vectors.size());
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    // Batch insert
    ASSERT_EQ(flat_index.build(records), ErrorCode::Ok);
    EXPECT_EQ(flat_index.size(), dataset_size_);

    // Verify search works
    std::vector<float> query(config_.dimension, 0.5f);
    auto results = flat_index.search(query, 10, SearchParams{});

    std::size_t expected_size = std::min(std::size_t{10}, dataset_size_);
    EXPECT_EQ(results.size(), expected_size);
}

TEST_P(FlatIndexEndToEndTest, SerializationRoundTrip) {
    FlatIndex flat_index1(config_.dimension, config_.distance_metric);

    // Insert vectors
    auto vectors = generate_random_vectors(dataset_size_, config_.dimension);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        flat_index1.add(i, vectors[i]);
    }

    // Serialize
    std::stringstream ss;
    ASSERT_EQ(flat_index1.serialize(ss), ErrorCode::Ok);

    // Deserialize
    FlatIndex flat_index2(config_.dimension, config_.distance_metric);
    ASSERT_EQ(flat_index2.deserialize(ss), ErrorCode::Ok);

    // Verify
    EXPECT_EQ(flat_index2.size(), dataset_size_);
    EXPECT_EQ(flat_index2.dimension(), config_.dimension);

    // Search should give identical results
    std::vector<float> query(config_.dimension, 0.5f);
    auto results1 = flat_index1.search(query, 10, SearchParams{});
    auto results2 = flat_index2.search(query, 10, SearchParams{});

    EXPECT_TRUE(results_equal(results1, results2));
}

TEST_P(FlatIndexEndToEndTest, AllDistanceMetrics) {
    for (auto metric : {DistanceMetric::L2, DistanceMetric::Cosine, DistanceMetric::DotProduct}) {
        FlatIndex flat_index(config_.dimension, metric);

        // Use normalized vectors for Cosine
        auto vectors = (metric == DistanceMetric::Cosine)
            ? generate_normalized_vectors(dataset_size_, config_.dimension)
            : generate_random_vectors(dataset_size_, config_.dimension);

        // Insert
        for (std::size_t i = 0; i < vectors.size(); ++i) {
            ASSERT_EQ(flat_index.add(i, vectors[i]), ErrorCode::Ok);
        }

        // Search
        std::vector<float> query = (metric == DistanceMetric::Cosine)
            ? generate_normalized_vectors(1, config_.dimension)[0]
            : generate_random_vectors(1, config_.dimension)[0];

        auto results = flat_index.search(query, 10, SearchParams{});

        std::size_t expected_size = std::min(std::size_t{10}, dataset_size_);
        EXPECT_EQ(results.size(), expected_size);

        // Verify sorted
        for (std::size_t i = 1; i < results.size(); ++i) {
            EXPECT_LE(results[i-1].distance, results[i].distance);
        }
    }
}

// Instantiate tests with different dataset sizes
INSTANTIATE_TEST_SUITE_P(
    DatasetSizes,
    FlatIndexEndToEndTest,
    ::testing::Values(1000, 10000, 100000)
);

// ============================================================================
// Performance Characteristics Documentation Test
// ============================================================================

TEST(FlatIndexDocumentationTest, PerformanceCharacteristics) {
    std::cout << "\n";
    std::cout << "===============================================================\n";
    std::cout << "           FlatIndex Performance Characteristics              \n";
    std::cout << "===============================================================\n\n";

    std::cout << "Query Complexity:       O(N·D)\n";
    std::cout << "  N = number of vectors\n";
    std::cout << "  D = vector dimension\n\n";

    std::cout << "Construction Complexity: O(1)\n";
    std::cout << "  No index building required\n\n";

    std::cout << "Memory Usage:           O(N·D)\n";
    std::cout << "  Stores only raw vector data\n\n";

    std::cout << "Recall:                 100%\n";
    std::cout << "  Exact nearest neighbor search\n\n";

    std::cout << "Best Use Cases:\n";
    std::cout << "  - Small datasets (<10K vectors)\n";
    std::cout << "  - Validation/ground truth for approximate methods\n";
    std::cout << "  - Applications requiring exact search\n";
    std::cout << "  - Fast index construction critical\n\n";

    std::cout << "===============================================================\n\n";

    // This test always passes - it's just for documentation
    SUCCEED();
}
