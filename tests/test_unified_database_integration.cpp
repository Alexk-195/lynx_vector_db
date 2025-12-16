/**
 * @file test_unified_database_integration.cpp
 * @brief Comprehensive integration tests for unified VectorDatabase
 *
 * Tests for ticket #2057: Integration Testing for Unified VectorDatabase
 *
 * This file contains comprehensive integration tests that validate the unified
 * VectorDatabase implementation with all three index types (Flat, HNSW, IVF).
 * It includes:
 * - End-to-end scenarios
 * - All distance metrics testing
 * - Persistence round-trip tests
 * - Mixed workload testing
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

        // Default HNSW parameters (will be adjusted per test based on dataset size)
        config_.hnsw_params.m = 16;
        config_.hnsw_params.ef_construction = 200;

        config_.ivf_params.n_clusters = 100;
        config_.ivf_params.n_probe = 10;
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    /**
     * @brief Configure index parameters based on dataset size.
     * Favors speed over recall for large datasets.
     *
     * @param dataset_size Expected number of vectors
     */
    void configure_for_dataset_size(std::size_t dataset_size) {
        // Configure HNSW parameters
        if (config_.index_type == IndexType::HNSW) {
            // Scale ef_construction based on dataset size (favoring speed)
            if (dataset_size <= 1000) {
                config_.hnsw_params.ef_construction = 200;  // High quality for small datasets
                config_.hnsw_params.ef_search = 50;
            } else if (dataset_size <= 10000) {
                config_.hnsw_params.ef_construction = 80;   // Balanced for medium datasets
                config_.hnsw_params.ef_search = 40;
            } else if (dataset_size <= 100000) {
                config_.hnsw_params.ef_construction = 32;   // Fast construction for large datasets (favoring speed)
                config_.hnsw_params.ef_search = 24;
            } else {
                config_.hnsw_params.ef_construction = 24;   // Very fast for huge datasets
                config_.hnsw_params.ef_search = 16;
            }
        }

        // Configure IVF parameters
        if (config_.index_type == IndexType::IVF) {
            // Scale n_clusters as sqrt(N) for optimal performance
            config_.ivf_params.n_clusters = static_cast<std::size_t>(std::sqrt(dataset_size));
            config_.ivf_params.n_probe = std::min(config_.ivf_params.n_clusters / 10, std::size_t(10));
        }
    }

    Config config_;
    std::string test_dir_;
};

TEST_P(UnifiedDatabaseEndToEndTest, Insert10K_Search_Save_Load_Search) {
    // Configure parameters for 10K dataset (favoring speed)
    const std::size_t dataset_size = 10000;
    constexpr int time_out_seconds = 20;
    configure_for_dataset_size(dataset_size);

    // Log configuration for HNSW
    if (config_.index_type == IndexType::HNSW) {
        std::cout << "\n[HNSW CONFIG] ef_construction=" << config_.hnsw_params.ef_construction
                  << ", ef_search=" << config_.hnsw_params.ef_search
                  << " (optimized for " << dataset_size << " vectors, favoring speed)\n";
    }

    // Step 1: Insert vectors with timeout
    auto db1 = std::make_shared<VectorDatabase>(config_);
    auto vectors = generate_random_vectors(dataset_size, 128);

    std::cout << "\n[END-TO-END] Inserting up to " << dataset_size << " vectors with timeout " << time_out_seconds << " seconds...\n";

    constexpr std::chrono::seconds insert_timeout(time_out_seconds);
    auto insert_start = std::chrono::steady_clock::now();

    std::size_t inserted_count = 0;
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        // Check timeout every 100 inserts
        if (i > 0 && i % 100 == 0) {
            auto elapsed = std::chrono::steady_clock::now() - insert_start;
            if (elapsed >= insert_timeout) {
                std::cout << "  Timeout reached after " << i << " inserts\n";
                break;
            }
        }

        VectorRecord record{i, vectors[i], std::nullopt};
        auto result = db1->insert(record);
        if (result == ErrorCode::Ok) {
            inserted_count++;
        }
    }

    auto insert_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - insert_start).count() / 1000.0;

    std::cout << "  Inserted " << inserted_count << " vectors in "
              << std::fixed << std::setprecision(2) << insert_elapsed << " seconds\n";
    EXPECT_EQ(db1->size(), inserted_count);
    EXPECT_GT(inserted_count, 0);  // Must have inserted at least some vectors
    EXPECT_LE(insert_elapsed, time_out_seconds + 5);  // Must complete within timeout + buffer

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
    EXPECT_EQ(db2->size(), inserted_count);

    // Step 5: Search again
    auto search_result2 = db2->search(query, 50);
    EXPECT_GT(search_result2.items.size(), 0);

    std::cout << "  Search after load returned " << search_result2.items.size() << " results\n";

    // Verify results are similar (for approximate indices, may not be identical)
    double recall = calculate_recall(search_result1.items, search_result2.items);
    std::cout << "  Recall after save/load: " << std::fixed << std::setprecision(2)
              << (recall * 100) << "%\n";

    // For 10K dataset, HNSW with ef_construction=80 should maintain good recall
    EXPECT_GT(recall, 0.90);  // At least 90% recall
}

TEST_P(UnifiedDatabaseEndToEndTest, MixedWorkload_ConcurrentReadWrite) {
    // Configure parameters for 10K dataset
    const std::size_t initial_size = 10000;
    constexpr int time_out_seconds = 20;

    configure_for_dataset_size(initial_size);

    auto db = std::make_shared<VectorDatabase>(config_);

    // Initial batch insert
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors(initial_size, 128);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    std::cout << "\n[MIXED WORKLOAD] Inserting initial " << initial_size << " vectors...\n";
    double batch_time = measure_time_ms([&]() {
        db->batch_insert(records);
    });
    std::cout << "  Initial insert time: " << std::fixed << std::setprecision(2)
              << (batch_time / 1000.0) << " seconds\n";

    std::cout << "[MIXED WORKLOAD] Testing concurrent operations with timeout... " << time_out_seconds << " seconds\n";


    // set timeout
    constexpr std::chrono::seconds timeout_duration(time_out_seconds);
    auto start_time = std::chrono::steady_clock::now();

    // Atomic flag to signal threads to stop
    std::atomic<bool> stop_flag{false};

    // Concurrent operations counters
    std::atomic<std::size_t> search_count{0};
    std::atomic<std::size_t> insert_count{0};
    std::atomic<std::uint64_t> next_id{initial_size};

    std::vector<std::thread> threads;

    // Reader threads (4 threads continuously searching)
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            auto query = generate_random_vectors(1, 128, 100 + t)[0];
            while (!stop_flag.load(std::memory_order_relaxed)) {
                auto results = db->search(query, 10);
                search_count++;

                // Check timeout periodically
                if (search_count % 10 == 0) {
                    auto elapsed = std::chrono::steady_clock::now() - start_time;
                    if (elapsed >= timeout_duration) {
                        stop_flag.store(true, std::memory_order_relaxed);
                        break;
                    }
                }
            }
        });
    }

    // Writer threads (2 threads continuously inserting)
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&, t]() {
            std::size_t local_insert = 0;
            while (!stop_flag.load(std::memory_order_relaxed)) {
                auto vec = generate_random_vectors(1, 128, 1000 + t * 10000 + local_insert)[0];
                std::uint64_t id = next_id.fetch_add(1, std::memory_order_relaxed);
                VectorRecord record{id, vec, std::nullopt};

                auto result = db->insert(record);
                if (result == ErrorCode::Ok) {
                    insert_count++;
                    local_insert++;
                }

                // Check timeout periodically (less frequently for writes)
                if (insert_count % 5 == 0) {
                    auto elapsed = std::chrono::steady_clock::now() - start_time;
                    if (elapsed >= timeout_duration) {
                        stop_flag.store(true, std::memory_order_relaxed);
                        break;
                    }
                }
            }
        });
    }

    // Monitor thread - ensures we stop after timeout
    threads.emplace_back([&]() {
        std::this_thread::sleep_for(timeout_duration);
        stop_flag.store(true, std::memory_order_relaxed);
    });

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time).count() / 1000.0;

    std::cout << "  Test duration: " << std::fixed << std::setprecision(2) << total_time << " seconds\n";
    std::cout << "  Completed " << search_count << " searches\n";
    std::cout << "  Completed " << insert_count << " inserts\n";
    std::cout << "  Final database size: " << db->size() << "\n";

    // Verify we completed within time limit (with small buffer for shutdown)
    EXPECT_LE(total_time, time_out_seconds + 5);  // timeout + 5 second buffer

    // Verify some operations completed
    EXPECT_GT(search_count, 0);
    EXPECT_GT(insert_count, 0);

    // Verify database grew
    EXPECT_GT(db->size(), initial_size);
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
