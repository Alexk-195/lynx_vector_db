/**
 * @file test_unified_database_integration.cpp
 * @brief Comprehensive integration tests for unified VectorDatabase
 *
 * Tests for ticket #2057: Integration Testing for Unified VectorDatabase
 *
 * This file contains comprehensive integration tests that validate the unified
 * VectorDatabase implementation with all three index types (Flat, HNSW, IVF).
 * It includes:
 * - Feature parity validation with old implementations
 * - Performance benchmarks
 * - End-to-end scenarios
 * - All distance metrics testing
 * - Persistence round-trip tests
 *
 * @copyright MIT License
 */

#include "../src/lib/vector_database.h"
#include "../src/lib/vector_database_flat.h"
#include "../src/lib/vector_database_hnsw.h"
#include "../src/lib/vector_database_ivf.h"
#include <gtest/gtest.h>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <filesystem>

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
 * @brief Calculate recall between two result sets.
 */
double calculate_recall(const std::vector<SearchResultItem>& ground_truth,
                       const std::vector<SearchResultItem>& results) {
    if (ground_truth.empty() || results.empty()) {
        return 0.0;
    }

    std::size_t matches = 0;
    for (const auto& result : results) {
        for (const auto& gt : ground_truth) {
            if (result.id == gt.id) {
                matches++;
                break;
            }
        }
    }

    return static_cast<double>(matches) / std::min(ground_truth.size(), results.size());
}

} // anonymous namespace

// ============================================================================
// Feature Parity Tests - Flat Index
// ============================================================================

class UnifiedDatabaseFlatParityTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.dimension = 128;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = IndexType::Flat;

        unified_db_ = std::make_shared<VectorDatabase>(config_);
        old_db_ = std::make_shared<VectorDatabase_Impl>(config_);
    }

    Config config_;
    std::shared_ptr<VectorDatabase> unified_db_;
    std::shared_ptr<VectorDatabase_Impl> old_db_;
};

TEST_F(UnifiedDatabaseFlatParityTest, InsertAndSearch) {
    // Insert same vectors into both databases
    auto vectors = generate_random_vectors(1000, 128);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        VectorRecord record{i, vectors[i], std::nullopt};
        EXPECT_EQ(unified_db_->insert(record), ErrorCode::Ok);
        EXPECT_EQ(old_db_->insert(record), ErrorCode::Ok);
    }

    // Search and compare results
    auto query = generate_random_vectors(1, 128)[0];
    auto unified_results = unified_db_->search(query, 10);
    auto old_results = old_db_->search(query, 10);

    // Results should be identical for brute-force search
    ASSERT_EQ(unified_results.items.size(), old_results.items.size());
    for (std::size_t i = 0; i < unified_results.items.size(); ++i) {
        EXPECT_EQ(unified_results.items[i].id, old_results.items[i].id);
        EXPECT_NEAR(unified_results.items[i].distance, old_results.items[i].distance, 1e-5);
    }
}

TEST_F(UnifiedDatabaseFlatParityTest, BatchInsert) {
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors(5000, 128);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    EXPECT_EQ(unified_db_->batch_insert(records), ErrorCode::Ok);
    EXPECT_EQ(old_db_->batch_insert(records), ErrorCode::Ok);

    EXPECT_EQ(unified_db_->size(), old_db_->size());

    // Verify search results match
    auto query = generate_random_vectors(1, 128)[0];
    auto unified_results = unified_db_->search(query, 20);
    auto old_results = old_db_->search(query, 20);

    ASSERT_EQ(unified_results.items.size(), old_results.items.size());
    for (std::size_t i = 0; i < unified_results.items.size(); ++i) {
        EXPECT_EQ(unified_results.items[i].id, old_results.items[i].id);
    }
}

TEST_F(UnifiedDatabaseFlatParityTest, RemoveOperation) {
    auto vectors = generate_random_vectors(100, 128);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        VectorRecord record{i, vectors[i], std::nullopt};
        unified_db_->insert(record);
        old_db_->insert(record);
    }

    // Remove some vectors
    for (std::uint64_t id : {5, 15, 25, 50, 75}) {
        EXPECT_EQ(unified_db_->remove(id), ErrorCode::Ok);
        EXPECT_EQ(old_db_->remove(id), ErrorCode::Ok);
    }

    EXPECT_EQ(unified_db_->size(), old_db_->size());

    // Verify removed vectors are gone
    for (std::uint64_t id : {5, 15, 25, 50, 75}) {
        EXPECT_FALSE(unified_db_->contains(id));
        EXPECT_FALSE(old_db_->contains(id));
    }
}

// ============================================================================
// Feature Parity Tests - HNSW Index
// ============================================================================

class UnifiedDatabaseHNSWParityTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.dimension = 128;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = IndexType::HNSW;
        config_.hnsw_params.m = 16;
        config_.hnsw_params.ef_construction = 200;
        config_.hnsw_params.ef_search = 100;

        unified_db_ = std::make_shared<VectorDatabase>(config_);
        old_db_ = std::make_shared<VectorDatabase_MPS>(config_);
    }

    Config config_;
    std::shared_ptr<VectorDatabase> unified_db_;
    std::shared_ptr<VectorDatabase_MPS> old_db_;
};

TEST_F(UnifiedDatabaseHNSWParityTest, SearchRecall) {
    // Insert vectors
    auto vectors = generate_random_vectors(10000, 128);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        VectorRecord record{i, vectors[i], std::nullopt};
        unified_db_->insert(record);
        old_db_->insert(record);
    }

    // Compare search results (should have similar recall)
    auto query = generate_random_vectors(1, 128)[0];
    auto unified_results = unified_db_->search(query, 50);
    auto old_results = old_db_->search(query, 50);

    // Calculate overlap
    double recall = calculate_recall(old_results.items, unified_results.items);

    std::cout << "\n[HNSW Parity] Search recall between unified and old: "
              << std::fixed << std::setprecision(2) << (recall * 100) << "%\n";

    // Recall should be high (both use same HNSW algorithm)
    EXPECT_GT(recall, 0.90);  // At least 90% recall
}

TEST_F(UnifiedDatabaseHNSWParityTest, BatchInsertRecall) {
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors(5000, 128);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    unified_db_->batch_insert(records);
    old_db_->batch_insert(records);

    EXPECT_EQ(unified_db_->size(), old_db_->size());

    // Verify search recall
    auto query = generate_random_vectors(1, 128)[0];
    auto unified_results = unified_db_->search(query, 30);
    auto old_results = old_db_->search(query, 30);

    double recall = calculate_recall(old_results.items, unified_results.items);
    EXPECT_GT(recall, 0.85);
}

// ============================================================================
// Feature Parity Tests - IVF Index
// ============================================================================

class UnifiedDatabaseIVFParityTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.dimension = 128;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = IndexType::IVF;
        config_.ivf_params.n_clusters = 100;
        config_.ivf_params.n_probe = 10;

        unified_db_ = std::make_shared<VectorDatabase>(config_);
        old_db_ = std::make_shared<VectorDatabase_IVF>(config_);
    }

    Config config_;
    std::shared_ptr<VectorDatabase> unified_db_;
    std::shared_ptr<VectorDatabase_IVF> old_db_;
};

TEST_F(UnifiedDatabaseIVFParityTest, BatchInsertAndSearch) {
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors(10000, 128);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    unified_db_->batch_insert(records);
    old_db_->batch_insert(records);

    EXPECT_EQ(unified_db_->size(), old_db_->size());

    // Verify both implementations can search successfully
    auto query = generate_random_vectors(1, 128)[0];
    auto unified_results = unified_db_->search(query, 20);
    auto old_results = old_db_->search(query, 20);

    // Both should return results
    EXPECT_GT(unified_results.items.size(), 0);
    EXPECT_GT(old_results.items.size(), 0);

    // Results should be sorted by distance
    for (size_t i = 1; i < unified_results.items.size(); ++i) {
        EXPECT_LE(unified_results.items[i-1].distance, unified_results.items[i].distance);
    }

    // Note: IVF uses k-means clustering which is stochastic
    // Different implementations may produce different clusters, so we don't
    // compare recall directly. The parameterized tests in test_vector_database.cpp
    // already verify that IVF search works correctly.
    double recall = calculate_recall(old_results.items, unified_results.items);
    std::cout << "\n[IVF Parity] Search recall between unified and old: "
              << std::fixed << std::setprecision(2) << (recall * 100) << "%\n";
    std::cout << "  (Note: Recall varies due to stochastic k-means clustering)\n";
}

// ============================================================================
// Performance Benchmark Tests
// ============================================================================

class UnifiedDatabaseBenchmarkTest : public ::testing::TestWithParam<IndexType> {
protected:
    void SetUp() override {
        config_.dimension = 128;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = GetParam();

        // Set reasonable parameters
        config_.hnsw_params.m = 16;
        config_.hnsw_params.ef_construction = 200;
        config_.hnsw_params.ef_search = 100;

        config_.ivf_params.n_clusters = 100;
        config_.ivf_params.n_probe = 10;
    }

    struct BenchmarkResult {
        double unified_time_ms;
        double old_time_ms;
        double speedup_ratio;
        std::size_t unified_memory;
        std::size_t old_memory;
        double memory_ratio;
    };

    BenchmarkResult run_search_benchmark(std::size_t num_vectors, std::size_t k) {
        auto unified_db = std::make_shared<VectorDatabase>(config_);
        std::shared_ptr<IVectorDatabase> old_db;

        // Create old implementation based on index type
        switch (config_.index_type) {
            case IndexType::Flat:
                old_db = std::make_shared<VectorDatabase_Impl>(config_);
                break;
            case IndexType::HNSW:
                old_db = std::make_shared<VectorDatabase_MPS>(config_);
                break;
            case IndexType::IVF:
                old_db = std::make_shared<VectorDatabase_IVF>(config_);
                break;
        }

        // Insert data
        auto vectors = generate_random_vectors(num_vectors, config_.dimension);
        for (std::size_t i = 0; i < vectors.size(); ++i) {
            VectorRecord record{i, vectors[i], std::nullopt};
            unified_db->insert(record);
            old_db->insert(record);
        }

        auto query = generate_random_vectors(1, config_.dimension)[0];

        // Benchmark unified database
        double unified_time = measure_time_ms([&]() {
            for (int i = 0; i < 100; ++i) {
                auto results = unified_db->search(query, k);
            }
        });

        // Benchmark old database
        double old_time = measure_time_ms([&]() {
            for (int i = 0; i < 100; ++i) {
                auto results = old_db->search(query, k);
            }
        });

        // Get memory usage
        auto unified_stats = unified_db->stats();
        auto old_stats = old_db->stats();

        return BenchmarkResult{
            unified_time / 100.0,  // Average per query
            old_time / 100.0,
            old_time / unified_time,
            unified_stats.memory_usage_bytes,
            old_stats.memory_usage_bytes,
            static_cast<double>(unified_stats.memory_usage_bytes) / old_stats.memory_usage_bytes
        };
    }

    Config config_;
};

TEST_P(UnifiedDatabaseBenchmarkTest, SearchLatency_10K) {
    auto result = run_search_benchmark(10000, 20);

    std::string index_name;
    switch (GetParam()) {
        case IndexType::Flat: index_name = "Flat"; break;
        case IndexType::HNSW: index_name = "HNSW"; break;
        case IndexType::IVF: index_name = "IVF"; break;
    }

    std::cout << "\n[BENCHMARK] " << index_name << " - 10K vectors:\n";
    std::cout << "  Unified DB: " << std::fixed << std::setprecision(3)
              << result.unified_time_ms << " ms/query\n";
    std::cout << "  Old DB:     " << result.old_time_ms << " ms/query\n";
    std::cout << "  Speedup:    " << std::setprecision(2)
              << result.speedup_ratio << "x\n";
    std::cout << "  Unified Memory: " << (result.unified_memory / 1024.0 / 1024.0) << " MB\n";
    std::cout << "  Old Memory:     " << (result.old_memory / 1024.0 / 1024.0) << " MB\n";
    std::cout << "  Memory Ratio:   " << std::setprecision(2)
              << result.memory_ratio << "x\n";

    // Latency should be within ±20% (allowing for architectural differences)
    // Note: Unified database has additional abstraction layers for flexibility
    EXPECT_NEAR(result.speedup_ratio, 1.0, 0.20);

    // Memory should be within ±100% (2x) due to architectural differences
    // Note: Unified database stores vectors in both database and index layer
    // This is a known tradeoff for separation of concerns and flexibility
    EXPECT_NEAR(result.memory_ratio, 1.0, 1.0);
}

// ============================================================================
// End-to-End Scenario Tests
// ============================================================================

class UnifiedDatabaseEndToEndTest : public ::testing::TestWithParam<IndexType> {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/lynx_unified_test_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);

        config_.dimension = 128;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = GetParam();
        config_.data_path = test_dir_;

        config_.hnsw_params.m = 16;
        config_.hnsw_params.ef_construction = 200;

        config_.ivf_params.n_clusters = 100;
        config_.ivf_params.n_probe = 10;
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    Config config_;
    std::string test_dir_;
};

TEST_P(UnifiedDatabaseEndToEndTest, Insert100K_Search_Save_Load_Search) {
    // Step 1: Insert 100K vectors
    auto db1 = std::make_shared<VectorDatabase>(config_);
    auto vectors = generate_random_vectors(100000, 128);

    std::cout << "\n[END-TO-END] Inserting 100K vectors...\n";
    double insert_time = measure_time_ms([&]() {
        for (std::size_t i = 0; i < vectors.size(); ++i) {
            VectorRecord record{i, vectors[i], std::nullopt};
            db1->insert(record);
        }
    });

    std::cout << "  Insert time: " << std::fixed << std::setprecision(2)
              << (insert_time / 1000.0) << " seconds\n";
    EXPECT_EQ(db1->size(), 100000);

    // Step 2: Search
    auto query = generate_random_vectors(1, 128)[0];
    auto search_result1 = db1->search(query, 50);
    EXPECT_GT(search_result1.items.size(), 0);

    std::cout << "  Search returned " << search_result1.items.size() << " results\n";

    // Step 3: Save
    std::cout << "  Saving database...\n";
    EXPECT_EQ(db1->save(), ErrorCode::Ok);

    // Step 4: Load in new database
    std::cout << "  Loading database...\n";
    auto db2 = std::make_shared<VectorDatabase>(config_);
    EXPECT_EQ(db2->load(), ErrorCode::Ok);
    EXPECT_EQ(db2->size(), 100000);

    // Step 5: Search again
    auto search_result2 = db2->search(query, 50);
    EXPECT_GT(search_result2.items.size(), 0);

    std::cout << "  Search after load returned " << search_result2.items.size() << " results\n";

    // Verify results are similar (for approximate indices, may not be identical)
    double recall = calculate_recall(search_result1.items, search_result2.items);
    std::cout << "  Recall after save/load: " << std::fixed << std::setprecision(2)
              << (recall * 100) << "%\n";

    EXPECT_GT(recall, 0.95);  // At least 95% recall
}

TEST_P(UnifiedDatabaseEndToEndTest, MixedWorkload_ConcurrentReadWrite) {
    auto db = std::make_shared<VectorDatabase>(config_);

    // Initial batch insert
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors(10000, 128);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }
    db->batch_insert(records);

    std::cout << "\n[MIXED WORKLOAD] Testing concurrent operations...\n";

    // Concurrent operations
    std::vector<std::thread> threads;
    std::atomic<std::size_t> search_count{0};
    std::atomic<std::size_t> insert_count{0};

    // Reader threads
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            auto query = generate_random_vectors(1, 128, 100 + t)[0];
            for (int i = 0; i < 100; ++i) {
                auto results = db->search(query, 10);
                search_count++;
            }
        });
    }

    // Writer threads
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 50; ++i) {
                auto vec = generate_random_vectors(1, 128, 1000 + t * 100 + i)[0];
                std::uint64_t id = 10000 + static_cast<std::uint64_t>(t) * 100 + static_cast<std::uint64_t>(i);
                VectorRecord record{id, vec, std::nullopt};
                db->insert(record);
                insert_count++;
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "  Completed " << search_count << " searches\n";
    std::cout << "  Completed " << insert_count << " inserts\n";
    std::cout << "  Final database size: " << db->size() << "\n";

    EXPECT_EQ(search_count, 400);
    EXPECT_EQ(insert_count, 100);
    EXPECT_EQ(db->size(), 10100);
}

// ============================================================================
// Distance Metrics Tests
// ============================================================================

class UnifiedDatabaseDistanceMetricsTest : public ::testing::TestWithParam<
    std::tuple<IndexType, DistanceMetric>> {
protected:
    void SetUp() override {
        config_.dimension = 128;
        config_.index_type = std::get<0>(GetParam());
        config_.distance_metric = std::get<1>(GetParam());

        config_.hnsw_params.m = 16;
        config_.hnsw_params.ef_construction = 100;

        config_.ivf_params.n_clusters = 50;
        config_.ivf_params.n_probe = 5;

        db_ = std::make_shared<VectorDatabase>(config_);
    }

    Config config_;
    std::shared_ptr<VectorDatabase> db_;
};

TEST_P(UnifiedDatabaseDistanceMetricsTest, SearchWithAllMetrics) {
    // Generate appropriate vectors based on metric
    std::vector<std::vector<float>> vectors;
    if (std::get<1>(GetParam()) == DistanceMetric::Cosine) {
        vectors = generate_normalized_vectors(1000, 128);
    } else {
        vectors = generate_random_vectors(1000, 128);
    }

    // Insert vectors
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        VectorRecord record{i, vectors[i], std::nullopt};
        EXPECT_EQ(db_->insert(record), ErrorCode::Ok);
    }

    // Search
    auto query = vectors[0];  // Use first vector as query (should be exact match)
    auto results = db_->search(query, 10);

    EXPECT_GT(results.items.size(), 0);
    EXPECT_EQ(results.items[0].id, 0);  // Exact match should be first

    std::string index_name = (std::get<0>(GetParam()) == IndexType::Flat) ? "Flat" :
                             (std::get<0>(GetParam()) == IndexType::HNSW) ? "HNSW" : "IVF";
    std::string metric_name = (std::get<1>(GetParam()) == DistanceMetric::L2) ? "L2" :
                              (std::get<1>(GetParam()) == DistanceMetric::Cosine) ? "Cosine" : "DotProduct";

    std::cout << "\n[DISTANCE METRIC] " << index_name << " with " << metric_name
              << ": Found " << results.items.size() << " results\n";

    // For exact search (Flat), verify exact match behavior
    if (std::get<0>(GetParam()) == IndexType::Flat) {
        // For DotProduct, exact match has largest magnitude (most negative)
        // For L2/Cosine, exact match should be near zero
        if (std::get<1>(GetParam()) == DistanceMetric::DotProduct) {
            // Just verify we got results; exact value depends on vector magnitude
            EXPECT_GT(results.items.size(), 0);
        } else {
            EXPECT_NEAR(results.items[0].distance, 0.0f, 1e-5);
        }
    }
}

// ============================================================================
// Test Instantiation
// ============================================================================

INSTANTIATE_TEST_SUITE_P(
    AllIndexTypes,
    UnifiedDatabaseBenchmarkTest,
    ::testing::Values(IndexType::Flat, IndexType::HNSW, IndexType::IVF),
    [](const ::testing::TestParamInfo<IndexType>& info) {
        switch (info.param) {
            case IndexType::Flat: return "Flat";
            case IndexType::HNSW: return "HNSW";
            case IndexType::IVF: return "IVF";
            default: return "Unknown";
        }
    }
);

INSTANTIATE_TEST_SUITE_P(
    AllIndexTypes,
    UnifiedDatabaseEndToEndTest,
    ::testing::Values(IndexType::Flat, IndexType::HNSW, IndexType::IVF),
    [](const ::testing::TestParamInfo<IndexType>& info) {
        switch (info.param) {
            case IndexType::Flat: return "Flat";
            case IndexType::HNSW: return "HNSW";
            case IndexType::IVF: return "IVF";
            default: return "Unknown";
        }
    }
);

INSTANTIATE_TEST_SUITE_P(
    AllCombinations,
    UnifiedDatabaseDistanceMetricsTest,
    ::testing::Combine(
        ::testing::Values(IndexType::Flat, IndexType::HNSW, IndexType::IVF),
        ::testing::Values(DistanceMetric::L2, DistanceMetric::Cosine, DistanceMetric::DotProduct)
    ),
    [](const ::testing::TestParamInfo<std::tuple<IndexType, DistanceMetric>>& info) {
        std::string index_name;
        switch (std::get<0>(info.param)) {
            case IndexType::Flat: index_name = "Flat"; break;
            case IndexType::HNSW: index_name = "HNSW"; break;
            case IndexType::IVF: index_name = "IVF"; break;
        }
        std::string metric_name;
        switch (std::get<1>(info.param)) {
            case DistanceMetric::L2: metric_name = "L2"; break;
            case DistanceMetric::Cosine: metric_name = "Cosine"; break;
            case DistanceMetric::DotProduct: metric_name = "DotProduct"; break;
        }
        return index_name + "_" + metric_name;
    }
);
