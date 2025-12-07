/**
 * @file test_hnsw.cpp
 * @brief Unit tests for HNSW index implementation
 *
 * @copyright MIT License
 */

#include <gtest/gtest.h>
#include "../src/lib/hnsw_index.h"
#include <random>
#include <algorithm>
#include <cmath>

using namespace lynx;

// ============================================================================
// Test Fixtures
// ============================================================================

class HNSWIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Default parameters for tests
        params_.m = 16;
        params_.ef_construction = 200;
        params_.ef_search = 50;
        params_.max_elements = 1000000;
    }

    HNSWParams params_;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generate random vector with specified dimension.
 */
std::vector<float> generate_random_vector(std::size_t dim, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> vec(dim);
    for (auto& v : vec) {
        v = dist(rng);
    }
    return vec;
}

/**
 * @brief Normalize vector to unit length.
 */
void normalize_vector(std::vector<float>& vec) {
    float norm = 0.0f;
    for (auto v : vec) {
        norm += v * v;
    }
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (auto& v : vec) {
            v /= norm;
        }
    }
}

/**
 * @brief Calculate L2 distance between two vectors.
 */
float l2_distance(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return std::sqrt(sum);
}

/**
 * @brief Find k nearest neighbors by brute force (ground truth).
 */
std::vector<SearchResultItem> brute_force_search(
    const std::vector<float>& query,
    const std::vector<std::pair<std::uint64_t, std::vector<float>>>& vectors,
    std::size_t k,
    DistanceMetric metric = DistanceMetric::L2) {

    std::vector<SearchResultItem> results;
    results.reserve(vectors.size());

    for (const auto& [id, vec] : vectors) {
        float dist = l2_distance(query, vec);
        results.push_back({id, dist});
    }

    // Sort by distance
    std::sort(results.begin(), results.end(),
              [](const SearchResultItem& a, const SearchResultItem& b) {
                  return a.distance < b.distance;
              });

    // Return top k
    if (results.size() > k) {
        results.resize(k);
    }

    return results;
}

// ============================================================================
// Basic Construction Tests
// ============================================================================

TEST_F(HNSWIndexTest, ConstructorBasic) {
    HNSWIndex index(128, DistanceMetric::L2, params_);
    EXPECT_EQ(index.dimension(), 128);
    EXPECT_EQ(index.size(), 0);
}

TEST_F(HNSWIndexTest, ConstructorDifferentDimensions) {
    HNSWIndex index1(64, DistanceMetric::L2, params_);
    HNSWIndex index2(256, DistanceMetric::Cosine, params_);
    HNSWIndex index3(1536, DistanceMetric::DotProduct, params_);

    EXPECT_EQ(index1.dimension(), 64);
    EXPECT_EQ(index2.dimension(), 256);
    EXPECT_EQ(index3.dimension(), 1536);
}

// ============================================================================
// Single Insert Tests
// ============================================================================

TEST_F(HNSWIndexTest, InsertSingleVector) {
    HNSWIndex index(3, DistanceMetric::L2, params_);
    std::vector<float> vec = {1.0f, 2.0f, 3.0f};

    ErrorCode err = index.add(1, vec);
    EXPECT_EQ(err, ErrorCode::Ok);
    EXPECT_EQ(index.size(), 1);
    EXPECT_TRUE(index.contains(1));
}

TEST_F(HNSWIndexTest, InsertMultipleVectors) {
    HNSWIndex index(3, DistanceMetric::L2, params_);

    std::vector<float> vec1 = {1.0f, 0.0f, 0.0f};
    std::vector<float> vec2 = {0.0f, 1.0f, 0.0f};
    std::vector<float> vec3 = {0.0f, 0.0f, 1.0f};

    EXPECT_EQ(index.add(1, vec1), ErrorCode::Ok);
    EXPECT_EQ(index.add(2, vec2), ErrorCode::Ok);
    EXPECT_EQ(index.add(3, vec3), ErrorCode::Ok);

    EXPECT_EQ(index.size(), 3);
    EXPECT_TRUE(index.contains(1));
    EXPECT_TRUE(index.contains(2));
    EXPECT_TRUE(index.contains(3));
}

TEST_F(HNSWIndexTest, InsertDimensionMismatch) {
    HNSWIndex index(3, DistanceMetric::L2, params_);
    std::vector<float> vec_wrong = {1.0f, 2.0f}; // Wrong dimension

    ErrorCode err = index.add(1, vec_wrong);
    EXPECT_EQ(err, ErrorCode::DimensionMismatch);
    EXPECT_EQ(index.size(), 0);
}

TEST_F(HNSWIndexTest, InsertDuplicateId) {
    HNSWIndex index(3, DistanceMetric::L2, params_);
    std::vector<float> vec1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> vec2 = {4.0f, 5.0f, 6.0f};

    EXPECT_EQ(index.add(1, vec1), ErrorCode::Ok);
    EXPECT_EQ(index.add(1, vec2), ErrorCode::InvalidState); // Duplicate ID
    EXPECT_EQ(index.size(), 1);
}

// ============================================================================
// Search Tests
// ============================================================================

TEST_F(HNSWIndexTest, SearchEmptyIndex) {
    HNSWIndex index(3, DistanceMetric::L2, params_);
    std::vector<float> query = {1.0f, 2.0f, 3.0f};

    SearchParams search_params;
    auto results = index.search(query, 5, search_params);

    EXPECT_TRUE(results.empty());
}

TEST_F(HNSWIndexTest, SearchSingleVector) {
    HNSWIndex index(3, DistanceMetric::L2, params_);
    std::vector<float> vec = {1.0f, 2.0f, 3.0f};

    index.add(1, vec);

    std::vector<float> query = {1.1f, 2.1f, 3.1f}; // Close to inserted vector
    SearchParams search_params;
    auto results = index.search(query, 1, search_params);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, 1);
    EXPECT_GT(results[0].distance, 0.0f);
    EXPECT_LT(results[0].distance, 0.3f);
}

TEST_F(HNSWIndexTest, SearchExactMatch) {
    HNSWIndex index(3, DistanceMetric::L2, params_);
    std::vector<float> vec = {1.0f, 2.0f, 3.0f};

    index.add(1, vec);

    SearchParams search_params;
    auto results = index.search(vec, 1, search_params); // Query with exact vector

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, 1);
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-6);
}

TEST_F(HNSWIndexTest, SearchMultipleVectorsOrdering) {
    HNSWIndex index(3, DistanceMetric::L2, params_);

    // Insert vectors at different distances from origin
    std::vector<float> vec1 = {1.0f, 0.0f, 0.0f};
    std::vector<float> vec2 = {2.0f, 0.0f, 0.0f};
    std::vector<float> vec3 = {3.0f, 0.0f, 0.0f};

    index.add(1, vec1);
    index.add(2, vec2);
    index.add(3, vec3);

    std::vector<float> query = {0.0f, 0.0f, 0.0f};
    SearchParams search_params;
    auto results = index.search(query, 3, search_params);

    ASSERT_EQ(results.size(), 3);

    // Results should be ordered by distance (closest first)
    EXPECT_EQ(results[0].id, 1);
    EXPECT_EQ(results[1].id, 2);
    EXPECT_EQ(results[2].id, 3);

    EXPECT_NEAR(results[0].distance, 1.0f, 1e-5);
    EXPECT_NEAR(results[1].distance, 2.0f, 1e-5);
    EXPECT_NEAR(results[2].distance, 3.0f, 1e-5);
}

TEST_F(HNSWIndexTest, SearchWithDifferentK) {
    HNSWIndex index(3, DistanceMetric::L2, params_);

    for (std::uint64_t i = 1; i <= 10; ++i) {
        std::vector<float> vec = {static_cast<float>(i), 0.0f, 0.0f};
        index.add(i, vec);
    }

    std::vector<float> query = {0.0f, 0.0f, 0.0f};
    SearchParams search_params;

    // Search with k=3
    auto results3 = index.search(query, 3, search_params);
    EXPECT_EQ(results3.size(), 3);

    // Search with k=5
    auto results5 = index.search(query, 5, search_params);
    EXPECT_EQ(results5.size(), 5);

    // Search with k=all
    auto results10 = index.search(query, 10, search_params);
    EXPECT_EQ(results10.size(), 10);
}

TEST_F(HNSWIndexTest, SearchDimensionMismatch) {
    HNSWIndex index(3, DistanceMetric::L2, params_);
    std::vector<float> vec = {1.0f, 2.0f, 3.0f};
    index.add(1, vec);

    std::vector<float> query_wrong = {1.0f, 2.0f}; // Wrong dimension
    SearchParams search_params;
    auto results = index.search(query_wrong, 1, search_params);

    EXPECT_TRUE(results.empty());
}

// ============================================================================
// Remove Tests
// ============================================================================

TEST_F(HNSWIndexTest, RemoveSingleVector) {
    HNSWIndex index(3, DistanceMetric::L2, params_);
    std::vector<float> vec = {1.0f, 2.0f, 3.0f};

    index.add(1, vec);
    EXPECT_TRUE(index.contains(1));

    ErrorCode err = index.remove(1);
    EXPECT_EQ(err, ErrorCode::Ok);
    EXPECT_FALSE(index.contains(1));
    EXPECT_EQ(index.size(), 0);
}

TEST_F(HNSWIndexTest, RemoveNonexistent) {
    HNSWIndex index(3, DistanceMetric::L2, params_);

    ErrorCode err = index.remove(999);
    EXPECT_EQ(err, ErrorCode::VectorNotFound);
}

TEST_F(HNSWIndexTest, RemoveAndSearchAgain) {
    HNSWIndex index(3, DistanceMetric::L2, params_);

    std::vector<float> vec1 = {1.0f, 0.0f, 0.0f};
    std::vector<float> vec2 = {2.0f, 0.0f, 0.0f};
    std::vector<float> vec3 = {3.0f, 0.0f, 0.0f};

    index.add(1, vec1);
    index.add(2, vec2);
    index.add(3, vec3);

    // Remove middle vector
    index.remove(2);

    std::vector<float> query = {0.0f, 0.0f, 0.0f};
    SearchParams search_params;
    auto results = index.search(query, 3, search_params);

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].id, 1);
    EXPECT_EQ(results[1].id, 3);
}

// ============================================================================
// Batch Build Tests
// ============================================================================

TEST_F(HNSWIndexTest, BatchBuildEmpty) {
    HNSWIndex index(3, DistanceMetric::L2, params_);
    std::vector<VectorRecord> records;

    ErrorCode err = index.build(records);
    EXPECT_EQ(err, ErrorCode::Ok);
    EXPECT_EQ(index.size(), 0);
}

TEST_F(HNSWIndexTest, BatchBuildMultiple) {
    HNSWIndex index(3, DistanceMetric::L2, params_);

    std::vector<VectorRecord> records = {
        {1, {1.0f, 0.0f, 0.0f}, std::nullopt},
        {2, {0.0f, 1.0f, 0.0f}, std::nullopt},
        {3, {0.0f, 0.0f, 1.0f}, std::nullopt},
    };

    ErrorCode err = index.build(records);
    EXPECT_EQ(err, ErrorCode::Ok);
    EXPECT_EQ(index.size(), 3);
    EXPECT_TRUE(index.contains(1));
    EXPECT_TRUE(index.contains(2));
    EXPECT_TRUE(index.contains(3));
}

// ============================================================================
// Recall Tests (Approximate Search Quality)
// ============================================================================

TEST_F(HNSWIndexTest, RecallTestSmallDataset) {
    constexpr std::size_t dim = 16;
    constexpr std::size_t num_vectors = 100;
    constexpr std::size_t num_queries = 10;
    constexpr std::size_t k = 10;

    std::mt19937 rng(42);

    HNSWIndex index(dim, DistanceMetric::L2, params_);

    // Generate and insert random vectors
    std::vector<std::pair<std::uint64_t, std::vector<float>>> vectors;
    for (std::uint64_t i = 0; i < num_vectors; ++i) {
        auto vec = generate_random_vector(dim, rng);
        vectors.push_back({i, vec});
        index.add(i, vec);
    }

    // Test queries
    std::size_t total_recall = 0;
    for (std::size_t q = 0; q < num_queries; ++q) {
        auto query = generate_random_vector(dim, rng);

        // HNSW search
        SearchParams search_params;
        auto hnsw_results = index.search(query, k, search_params);

        // Brute force search (ground truth)
        auto true_results = brute_force_search(query, vectors, k);

        // Calculate recall
        std::unordered_set<std::uint64_t> true_ids;
        for (const auto& item : true_results) {
            true_ids.insert(item.id);
        }

        std::size_t recall = 0;
        for (const auto& item : hnsw_results) {
            if (true_ids.find(item.id) != true_ids.end()) {
                ++recall;
            }
        }

        total_recall += recall;
    }

    // Calculate average recall
    double avg_recall = static_cast<double>(total_recall) / (num_queries * k);

    // HNSW should achieve high recall (>90%) even on small datasets
    EXPECT_GT(avg_recall, 0.90) << "Average recall: " << avg_recall;
}

TEST_F(HNSWIndexTest, RecallTestWithDifferentEfSearch) {
    constexpr std::size_t dim = 16;
    constexpr std::size_t num_vectors = 100;
    constexpr std::size_t k = 10;

    std::mt19937 rng(42);

    HNSWIndex index(dim, DistanceMetric::L2, params_);

    // Generate and insert random vectors
    std::vector<std::pair<std::uint64_t, std::vector<float>>> vectors;
    for (std::uint64_t i = 0; i < num_vectors; ++i) {
        auto vec = generate_random_vector(dim, rng);
        vectors.push_back({i, vec});
        index.add(i, vec);
    }

    auto query = generate_random_vector(dim, rng);
    auto true_results = brute_force_search(query, vectors, k);

    std::unordered_set<std::uint64_t> true_ids;
    for (const auto& item : true_results) {
        true_ids.insert(item.id);
    }

    // Test with ef_search = 10 (low)
    SearchParams params_low;
    params_low.ef_search = 10;
    auto results_low = index.search(query, k, params_low);

    std::size_t recall_low = 0;
    for (const auto& item : results_low) {
        if (true_ids.find(item.id) != true_ids.end()) {
            ++recall_low;
        }
    }

    // Test with ef_search = 100 (high)
    SearchParams params_high;
    params_high.ef_search = 100;
    auto results_high = index.search(query, k, params_high);

    std::size_t recall_high = 0;
    for (const auto& item : results_high) {
        if (true_ids.find(item.id) != true_ids.end()) {
            ++recall_high;
        }
    }

    // Higher ef_search should give better or equal recall
    EXPECT_GE(recall_high, recall_low);
}

// ============================================================================
// Distance Metric Tests
// ============================================================================

TEST_F(HNSWIndexTest, L2DistanceMetric) {
    HNSWIndex index(3, DistanceMetric::L2, params_);

    std::vector<float> vec1 = {1.0f, 0.0f, 0.0f};
    std::vector<float> vec2 = {0.0f, 1.0f, 0.0f};

    index.add(1, vec1);
    index.add(2, vec2);

    std::vector<float> query = {1.0f, 0.0f, 0.0f};
    SearchParams search_params;
    auto results = index.search(query, 2, search_params);

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].id, 1); // Exact match should be first
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-6);
}

TEST_F(HNSWIndexTest, CosineDistanceMetric) {
    HNSWIndex index(3, DistanceMetric::Cosine, params_);

    // Use normalized vectors for cosine similarity
    std::vector<float> vec1 = {1.0f, 0.0f, 0.0f};
    std::vector<float> vec2 = {0.0f, 1.0f, 0.0f};

    index.add(1, vec1);
    index.add(2, vec2);

    std::vector<float> query = {1.0f, 0.0f, 0.0f};
    SearchParams search_params;
    auto results = index.search(query, 2, search_params);

    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].id, 1);
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-6);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(HNSWIndexTest, SearchWithKLargerThanSize) {
    HNSWIndex index(3, DistanceMetric::L2, params_);

    std::vector<float> vec1 = {1.0f, 0.0f, 0.0f};
    std::vector<float> vec2 = {2.0f, 0.0f, 0.0f};

    index.add(1, vec1);
    index.add(2, vec2);

    std::vector<float> query = {0.0f, 0.0f, 0.0f};
    SearchParams search_params;
    auto results = index.search(query, 100, search_params); // k > size

    EXPECT_EQ(results.size(), 2); // Should return all available vectors
}

TEST_F(HNSWIndexTest, ZeroVectors) {
    HNSWIndex index(3, DistanceMetric::L2, params_);

    std::vector<float> zero_vec = {0.0f, 0.0f, 0.0f};
    index.add(1, zero_vec);

    std::vector<float> query = {0.0f, 0.0f, 0.0f};
    SearchParams search_params;
    auto results = index.search(query, 1, search_params);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, 1);
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-6);
}
