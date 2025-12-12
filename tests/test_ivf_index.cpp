/**
 * @file test_ivf_index.cpp
 * @brief Unit tests for IVF (Inverted File Index) implementation
 *
 * Tests for ticket #2002: IVFIndex Class Structure and Basic Operations
 *
 * @copyright MIT License
 */

#include "../src/lib/ivf_index.h"
#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <random>
#include <thread>
#include <algorithm>

using namespace lynx;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generate random vectors for testing.
 */
std::vector<std::vector<float>> generate_random_vectors_ivf(
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
 * @brief Generate well-separated centroids for testing.
 */
std::vector<std::vector<float>> generate_test_centroids(
    std::size_t k, std::size_t dimension, float separation = 10.0f) {
    std::vector<std::vector<float>> centroids;
    centroids.reserve(k);

    for (std::size_t i = 0; i < k; ++i) {
        std::vector<float> centroid(dimension, 0.0f);
        // Place centroids along first axis with separation
        centroid[0] = static_cast<float>(i) * separation;
        centroids.push_back(std::move(centroid));
    }

    return centroids;
}

/**
 * @brief Generate vectors near a specific centroid.
 */
std::vector<std::vector<float>> generate_vectors_near_centroid(
    const std::vector<float>& centroid, std::size_t count,
    float noise = 0.5f, std::uint64_t seed = 42) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, noise);

    std::vector<std::vector<float>> vectors;
    vectors.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        std::vector<float> vec(centroid.size());
        for (std::size_t d = 0; d < centroid.size(); ++d) {
            vec[d] = centroid[d] + dist(rng);
        }
        vectors.push_back(std::move(vec));
    }

    return vectors;
}

// ============================================================================
// Constructor Tests
// ============================================================================

TEST(IVFIndexTest, ConstructorValid) {
    IVFParams params;
    params.n_clusters = 10;
    params.n_probe = 5;

    EXPECT_NO_THROW({
        IVFIndex index(64, DistanceMetric::L2, params);
        EXPECT_EQ(index.dimension(), 64);
        EXPECT_EQ(index.size(), 0);
        EXPECT_FALSE(index.has_centroids());
    });
}

TEST(IVFIndexTest, ConstructorDifferentMetrics) {
    IVFParams params;
    params.n_clusters = 10;

    EXPECT_NO_THROW({
        IVFIndex index_l2(64, DistanceMetric::L2, params);
    });

    EXPECT_NO_THROW({
        IVFIndex index_cosine(64, DistanceMetric::Cosine, params);
    });

    EXPECT_NO_THROW({
        IVFIndex index_dot(64, DistanceMetric::DotProduct, params);
    });
}

TEST(IVFIndexTest, ConstructorInvalidDimension) {
    IVFParams params;
    params.n_clusters = 10;

    EXPECT_THROW({
        IVFIndex index(0, DistanceMetric::L2, params);
    }, std::invalid_argument);
}

TEST(IVFIndexTest, ConstructorInvalidClusters) {
    IVFParams params;
    params.n_clusters = 0;

    EXPECT_THROW({
        IVFIndex index(64, DistanceMetric::L2, params);
    }, std::invalid_argument);
}

// ============================================================================
// SetCentroids Tests
// ============================================================================

TEST(IVFIndexTest, SetCentroidsValid) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);

    EXPECT_EQ(index.set_centroids(centroids), ErrorCode::Ok);
    EXPECT_TRUE(index.has_centroids());
    EXPECT_EQ(index.centroids().size(), 3);
}

TEST(IVFIndexTest, SetCentroidsEmpty) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::vector<std::vector<float>> empty_centroids;

    EXPECT_EQ(index.set_centroids(empty_centroids), ErrorCode::InvalidParameter);
    EXPECT_FALSE(index.has_centroids());
}

TEST(IVFIndexTest, SetCentroidsDimensionMismatch) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Create centroids with wrong dimension (4 instead of 8)
    auto centroids = generate_test_centroids(3, 4);

    EXPECT_EQ(index.set_centroids(centroids), ErrorCode::DimensionMismatch);
    EXPECT_FALSE(index.has_centroids());
}

TEST(IVFIndexTest, SetCentroidsOverwrite) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Set initial centroids
    auto centroids1 = generate_test_centroids(3, 8, 10.0f);
    EXPECT_EQ(index.set_centroids(centroids1), ErrorCode::Ok);

    // Add some vectors
    auto vectors = generate_vectors_near_centroid(centroids1[0], 5);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }
    EXPECT_EQ(index.size(), 5);

    // Set new centroids (should clear existing data)
    auto centroids2 = generate_test_centroids(5, 8, 20.0f);
    EXPECT_EQ(index.set_centroids(centroids2), ErrorCode::Ok);

    EXPECT_EQ(index.size(), 0);  // Vectors should be cleared
    EXPECT_EQ(index.centroids().size(), 5);  // New number of centroids
}

// ============================================================================
// Add Tests
// ============================================================================

TEST(IVFIndexTest, AddSingleVector) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Set centroids first
    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    // Add a vector
    std::vector<float> vec(8, 1.0f);
    EXPECT_EQ(index.add(1, vec), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 1);
    EXPECT_TRUE(index.contains(1));
}

TEST(IVFIndexTest, AddMultipleVectors) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    // Add 100 vectors
    auto vectors = generate_random_vectors_ivf(100, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        EXPECT_EQ(index.add(i, vectors[i]), ErrorCode::Ok);
    }

    EXPECT_EQ(index.size(), 100);
}

TEST(IVFIndexTest, AddWithoutCentroids) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Try to add without setting centroids
    std::vector<float> vec(8, 1.0f);
    EXPECT_EQ(index.add(1, vec), ErrorCode::InvalidState);
}

TEST(IVFIndexTest, AddDimensionMismatch) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    // Try to add vector with wrong dimension
    std::vector<float> vec(16, 1.0f);  // Wrong dimension
    EXPECT_EQ(index.add(1, vec), ErrorCode::DimensionMismatch);
    EXPECT_EQ(index.size(), 0);
}

TEST(IVFIndexTest, AddDuplicateId) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    std::vector<float> vec1(8, 1.0f);
    std::vector<float> vec2(8, 2.0f);

    EXPECT_EQ(index.add(1, vec1), ErrorCode::Ok);
    EXPECT_EQ(index.add(1, vec2), ErrorCode::InvalidState);  // Duplicate ID
    EXPECT_EQ(index.size(), 1);
}

TEST(IVFIndexTest, AddToCorrectCluster) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Create well-separated centroids
    auto centroids = generate_test_centroids(3, 8, 100.0f);
    index.set_centroids(centroids);

    // Add vectors very close to each centroid
    for (std::size_t c = 0; c < 3; ++c) {
        auto vecs = generate_vectors_near_centroid(centroids[c], 10, 0.1f, c);
        for (std::size_t i = 0; i < vecs.size(); ++i) {
            std::uint64_t id = c * 100 + i;
            EXPECT_EQ(index.add(id, vecs[i]), ErrorCode::Ok);
        }
    }

    EXPECT_EQ(index.size(), 30);
}

// ============================================================================
// Contains Tests
// ============================================================================

TEST(IVFIndexTest, ContainsExisting) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    std::vector<float> vec(8, 1.0f);
    index.add(42, vec);

    EXPECT_TRUE(index.contains(42));
}

TEST(IVFIndexTest, ContainsNonExisting) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    std::vector<float> vec(8, 1.0f);
    index.add(42, vec);

    EXPECT_FALSE(index.contains(99));
    EXPECT_FALSE(index.contains(0));
}

TEST(IVFIndexTest, ContainsEmptyIndex) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    EXPECT_FALSE(index.contains(1));
    EXPECT_FALSE(index.contains(0));
}

// ============================================================================
// Size, Dimension, MemoryUsage Tests
// ============================================================================

TEST(IVFIndexTest, SizeEmpty) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(64, DistanceMetric::L2, params);
    EXPECT_EQ(index.size(), 0);
}

TEST(IVFIndexTest, SizeAfterAdd) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    auto vectors = generate_random_vectors_ivf(50, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
        EXPECT_EQ(index.size(), i + 1);
    }
}

TEST(IVFIndexTest, DimensionReturnsCorrectValue) {
    IVFParams params;
    params.n_clusters = 10;

    std::vector<std::size_t> dims = {8, 64, 128, 256, 512};

    for (auto dim : dims) {
        IVFIndex index(dim, DistanceMetric::L2, params);
        EXPECT_EQ(index.dimension(), dim);
    }
}

TEST(IVFIndexTest, MemoryUsageIncreases) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(64, DistanceMetric::L2, params);

    std::size_t initial_memory = index.memory_usage();

    auto centroids = generate_test_centroids(3, 64);
    index.set_centroids(centroids);

    std::size_t after_centroids = index.memory_usage();
    EXPECT_GT(after_centroids, initial_memory);

    auto vectors = generate_random_vectors_ivf(100, 64);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    std::size_t after_vectors = index.memory_usage();
    EXPECT_GT(after_vectors, after_centroids);
}

// ============================================================================
// Search Tests (Ticket #2003)
// ============================================================================

TEST(IVFIndexTest, SearchBasic) {
    IVFParams params;
    params.n_clusters = 3;
    params.n_probe = 3;  // Search all clusters

    IVFIndex index(8, DistanceMetric::L2, params);

    // Set centroids
    auto centroids = generate_test_centroids(3, 8, 10.0f);
    index.set_centroids(centroids);

    // Add vectors
    auto vectors = generate_random_vectors_ivf(30, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    // Search for top-10 nearest neighbors
    std::vector<float> query(8, 0.0f);
    SearchParams search_params;
    search_params.n_probe = 3;

    auto results = index.search(query, 10, search_params);

    EXPECT_EQ(results.size(), 10);

    // Verify results are sorted by distance
    for (std::size_t i = 1; i < results.size(); ++i) {
        EXPECT_LE(results[i-1].distance, results[i].distance);
    }
}

TEST(IVFIndexTest, SearchEmptyIndex) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Set centroids but don't add any vectors
    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    std::vector<float> query(8, 0.0f);
    SearchParams search_params;

    auto results = index.search(query, 10, search_params);
    EXPECT_TRUE(results.empty());
}

TEST(IVFIndexTest, SearchWithoutCentroids) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::vector<float> query(8, 0.0f);
    SearchParams search_params;

    auto results = index.search(query, 10, search_params);
    EXPECT_TRUE(results.empty());
}

TEST(IVFIndexTest, SearchDimensionMismatch) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    std::vector<float> query(16, 0.0f);  // Wrong dimension
    SearchParams search_params;

    auto results = index.search(query, 10, search_params);
    EXPECT_TRUE(results.empty());
}

TEST(IVFIndexTest, SearchKLargerThanVectors) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    // Add only 5 vectors
    auto vectors = generate_random_vectors_ivf(5, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    std::vector<float> query(8, 0.0f);
    SearchParams search_params;
    search_params.n_probe = 3;

    // Request 10 neighbors, but only 5 exist
    auto results = index.search(query, 10, search_params);
    EXPECT_EQ(results.size(), 5);
}

TEST(IVFIndexTest, SearchWithNProbe1) {
    IVFParams params;
    params.n_clusters = 5;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Create well-separated centroids
    auto centroids = generate_test_centroids(5, 8, 100.0f);
    index.set_centroids(centroids);

    // Add vectors near each centroid
    for (std::size_t c = 0; c < 5; ++c) {
        auto vecs = generate_vectors_near_centroid(centroids[c], 10, 0.5f, c);
        for (std::size_t i = 0; i < vecs.size(); ++i) {
            index.add(c * 10 + i, vecs[i]);
        }
    }

    // Query near first centroid
    auto query = centroids[0];
    SearchParams search_params;
    search_params.n_probe = 1;  // Only search nearest cluster

    auto results = index.search(query, 5, search_params);

    // Should find 5 results from the nearest cluster
    EXPECT_EQ(results.size(), 5);
}

TEST(IVFIndexTest, SearchWithNProbeAll) {
    IVFParams params;
    params.n_clusters = 5;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(5, 8, 100.0f);
    index.set_centroids(centroids);

    // Add vectors
    for (std::size_t c = 0; c < 5; ++c) {
        auto vecs = generate_vectors_near_centroid(centroids[c], 10, 0.5f, c);
        for (std::size_t i = 0; i < vecs.size(); ++i) {
            index.add(c * 10 + i, vecs[i]);
        }
    }

    auto query = centroids[0];
    SearchParams search_params;
    search_params.n_probe = 5;  // Search all clusters

    auto results = index.search(query, 10, search_params);

    // Should find 10 results from all clusters
    EXPECT_EQ(results.size(), 10);
}

TEST(IVFIndexTest, SearchNProbeGreaterThanClusters) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    auto vectors = generate_random_vectors_ivf(30, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    std::vector<float> query(8, 0.0f);
    SearchParams search_params;
    search_params.n_probe = 100;  // More than number of clusters

    // Should clamp to 3 (num_clusters) and work correctly
    auto results = index.search(query, 10, search_params);
    EXPECT_EQ(results.size(), 10);
}

TEST(IVFIndexTest, SearchExactMatch) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    // Add a specific vector
    std::vector<float> target_vec = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    index.add(42, target_vec);

    // Add some other vectors
    auto vectors = generate_random_vectors_ivf(20, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(100 + i, vectors[i]);
    }

    // Search for exact match
    SearchParams search_params;
    search_params.n_probe = 3;
    auto results = index.search(target_vec, 1, search_params);

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, 42);
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-6f);  // Distance should be ~0
}

TEST(IVFIndexTest, SearchL2Metric) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(4, DistanceMetric::L2, params);

    std::vector<std::vector<float>> centroids = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {10.0f, 0.0f, 0.0f, 0.0f},
        {20.0f, 0.0f, 0.0f, 0.0f}
    };
    index.set_centroids(centroids);

    // Add vectors at known distances
    std::vector<float> vec1 = {0.0f, 0.0f, 0.0f, 0.0f};  // Distance 0 from query
    std::vector<float> vec2 = {1.0f, 0.0f, 0.0f, 0.0f};  // Distance 1 from query
    std::vector<float> vec3 = {2.0f, 0.0f, 0.0f, 0.0f};  // Distance 2 from query

    index.add(1, vec1);
    index.add(2, vec2);
    index.add(3, vec3);

    std::vector<float> query = {0.0f, 0.0f, 0.0f, 0.0f};
    SearchParams search_params;
    search_params.n_probe = 3;

    auto results = index.search(query, 3, search_params);

    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0].id, 1);  // Closest
    EXPECT_EQ(results[1].id, 2);
    EXPECT_EQ(results[2].id, 3);  // Farthest
}

TEST(IVFIndexTest, SearchCosineMetric) {
    IVFParams params;
    params.n_clusters = 2;

    IVFIndex index(4, DistanceMetric::Cosine, params);

    std::vector<std::vector<float>> centroids = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f}
    };
    index.set_centroids(centroids);

    // Vectors in similar directions
    std::vector<float> vec1 = {1.0f, 0.0f, 0.0f, 0.0f};   // Same direction as query
    std::vector<float> vec2 = {0.9f, 0.1f, 0.0f, 0.0f};   // Similar direction
    std::vector<float> vec3 = {0.0f, 1.0f, 0.0f, 0.0f};   // Orthogonal direction

    index.add(1, vec1);
    index.add(2, vec2);
    index.add(3, vec3);

    std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
    SearchParams search_params;
    search_params.n_probe = 2;

    auto results = index.search(query, 3, search_params);

    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0].id, 1);  // Same direction (smallest cosine distance)
}

TEST(IVFIndexTest, SearchDotProductMetric) {
    IVFParams params;
    params.n_clusters = 2;

    IVFIndex index(4, DistanceMetric::DotProduct, params);

    std::vector<std::vector<float>> centroids = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f, 0.0f}
    };
    index.set_centroids(centroids);

    std::vector<float> vec1 = {1.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> vec2 = {0.5f, 0.0f, 0.0f, 0.0f};
    std::vector<float> vec3 = {-1.0f, 0.0f, 0.0f, 0.0f};

    index.add(1, vec1);
    index.add(2, vec2);
    index.add(3, vec3);

    std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
    SearchParams search_params;
    search_params.n_probe = 2;

    auto results = index.search(query, 3, search_params);

    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0].id, 1);  // Largest dot product (smallest negative)
}

TEST(IVFIndexTest, SearchRecallVsNProbe) {
    // Test that higher n_probe gives better recall
    IVFParams params;
    params.n_clusters = 10;

    IVFIndex index(64, DistanceMetric::L2, params);

    // Generate dataset
    auto centroids = generate_test_centroids(10, 64, 50.0f);
    index.set_centroids(centroids);

    auto vectors = generate_random_vectors_ivf(1000, 64, 12345);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    // Pick a query vector
    std::vector<float> query = vectors[0];

    // Search with n_probe=1
    SearchParams params1;
    params1.n_probe = 1;
    auto results1 = index.search(query, 10, params1);

    // Search with n_probe=5
    SearchParams params5;
    params5.n_probe = 5;
    auto results5 = index.search(query, 10, params5);

    // Search with n_probe=10 (all clusters)
    SearchParams params10;
    params10.n_probe = 10;
    auto results10 = index.search(query, 10, params10);

    EXPECT_EQ(results1.size(), 10);
    EXPECT_EQ(results5.size(), 10);
    EXPECT_EQ(results10.size(), 10);

    // Higher n_probe should give same or better (lower) top distance
    EXPECT_LE(results10[0].distance, results5[0].distance);
    EXPECT_LE(results5[0].distance, results1[0].distance);
}

TEST(IVFIndexTest, SearchConcurrent) {
    IVFParams params;
    params.n_clusters = 5;

    IVFIndex index(64, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(5, 64);
    index.set_centroids(centroids);

    auto vectors = generate_random_vectors_ivf(100, 64);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    // Concurrent searches should not crash
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&index, &vectors]() {
            SearchParams search_params;
            search_params.n_probe = 3;
            for (int i = 0; i < 100; ++i) {
                auto results = index.search(vectors[i % vectors.size()], 10, search_params);
                EXPECT_LE(results.size(), 10);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

// ============================================================================
// Remove Tests (Ticket #2004)
// ============================================================================

TEST(IVFIndexTest, RemoveExistingVector) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    // Add vectors
    auto vectors = generate_random_vectors_ivf(10, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    EXPECT_EQ(index.size(), 10);
    EXPECT_TRUE(index.contains(5));

    // Remove vector
    EXPECT_EQ(index.remove(5), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 9);
    EXPECT_FALSE(index.contains(5));
}

TEST(IVFIndexTest, RemoveNonExistentVector) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    // Try to remove non-existent vector
    EXPECT_EQ(index.remove(999), ErrorCode::VectorNotFound);
}

TEST(IVFIndexTest, RemoveFromDifferentClusters) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Create well-separated centroids
    auto centroids = generate_test_centroids(3, 8, 100.0f);
    index.set_centroids(centroids);

    // Add vectors to different clusters
    for (std::size_t c = 0; c < 3; ++c) {
        auto vecs = generate_vectors_near_centroid(centroids[c], 5, 0.5f, c);
        for (std::size_t i = 0; i < vecs.size(); ++i) {
            index.add(c * 10 + i, vecs[i]);
        }
    }

    EXPECT_EQ(index.size(), 15);

    // Remove from each cluster
    EXPECT_EQ(index.remove(0), ErrorCode::Ok);   // Cluster 0
    EXPECT_EQ(index.remove(10), ErrorCode::Ok);  // Cluster 1
    EXPECT_EQ(index.remove(20), ErrorCode::Ok);  // Cluster 2

    EXPECT_EQ(index.size(), 12);
}

TEST(IVFIndexTest, RemoveAndSearch) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    // Add vectors
    auto vectors = generate_random_vectors_ivf(20, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    // Search before removal
    SearchParams search_params;
    search_params.n_probe = 3;
    auto results_before = index.search(vectors[0], 10, search_params);

    // Remove a vector
    index.remove(5);

    // Search after removal - should not contain removed vector
    auto results_after = index.search(vectors[0], 10, search_params);

    for (const auto& result : results_after) {
        EXPECT_NE(result.id, 5);
    }
}

// ============================================================================
// Build Tests (Ticket #2004)
// ============================================================================

TEST(IVFIndexTest, BuildWithSmallDataset) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Create vector records
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors_ivf(100, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    EXPECT_EQ(index.build(records), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 100);
    EXPECT_TRUE(index.has_centroids());
    EXPECT_EQ(index.centroids().size(), 3);
}

TEST(IVFIndexTest, BuildWithEmptyDataset) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::vector<VectorRecord> records;
    EXPECT_EQ(index.build(records), ErrorCode::InvalidParameter);
}

TEST(IVFIndexTest, BuildWithDimensionMismatch) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::vector<VectorRecord> records;
    std::vector<float> wrong_vec(16, 1.0f);  // Wrong dimension
    records.push_back({1, wrong_vec, std::nullopt});

    EXPECT_EQ(index.build(records), ErrorCode::DimensionMismatch);
}

TEST(IVFIndexTest, BuildAndSearch) {
    IVFParams params;
    params.n_clusters = 5;

    IVFIndex index(64, DistanceMetric::L2, params);

    // Build with dataset
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors_ivf(1000, 64, 42);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    EXPECT_EQ(index.build(records), ErrorCode::Ok);

    // Verify search works
    SearchParams search_params;
    search_params.n_probe = 3;
    auto results = index.search(vectors[0], 10, search_params);

    EXPECT_EQ(results.size(), 10);
    EXPECT_EQ(results[0].id, 0);  // Should find itself as nearest
}

TEST(IVFIndexTest, BuildOverwritesExistingData) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Set centroids and add vectors
    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    auto vectors1 = generate_random_vectors_ivf(10, 8);
    for (std::size_t i = 0; i < vectors1.size(); ++i) {
        index.add(i, vectors1[i]);
    }

    EXPECT_EQ(index.size(), 10);

    // Build with new dataset
    std::vector<VectorRecord> records;
    auto vectors2 = generate_random_vectors_ivf(20, 8, 99);
    for (std::size_t i = 0; i < vectors2.size(); ++i) {
        records.push_back({100 + i, vectors2[i], std::nullopt});
    }

    EXPECT_EQ(index.build(records), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 20);

    // Old vectors should be gone
    EXPECT_FALSE(index.contains(0));
    EXPECT_TRUE(index.contains(100));
}

// ============================================================================
// Serialization Tests (Ticket #2004)
// ============================================================================

TEST(IVFIndexTest, SerializeEmptyIndex) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    // Set centroids but no vectors
    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    std::ostringstream oss;
    EXPECT_EQ(index.serialize(oss), ErrorCode::Ok);
    EXPECT_GT(oss.str().size(), 0);
}

TEST(IVFIndexTest, SerializeWithData) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    auto vectors = generate_random_vectors_ivf(50, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    std::ostringstream oss;
    EXPECT_EQ(index.serialize(oss), ErrorCode::Ok);
    EXPECT_GT(oss.str().size(), 0);
}

TEST(IVFIndexTest, DeserializeValidData) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index1(8, DistanceMetric::L2, params);

    // Build and serialize
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors_ivf(100, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }
    index1.build(records);

    std::ostringstream oss;
    EXPECT_EQ(index1.serialize(oss), ErrorCode::Ok);

    // Deserialize
    IVFIndex index2(8, DistanceMetric::L2, params);
    std::istringstream iss(oss.str());
    EXPECT_EQ(index2.deserialize(iss), ErrorCode::Ok);

    // Verify state
    EXPECT_EQ(index2.size(), 100);
    EXPECT_TRUE(index2.has_centroids());
    EXPECT_EQ(index2.centroids().size(), 3);
}

TEST(IVFIndexTest, SerializeDeserializeRoundTrip) {
    IVFParams params;
    params.n_clusters = 5;

    IVFIndex index1(64, DistanceMetric::L2, params);

    // Build index
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors_ivf(200, 64, 12345);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }
    index1.build(records);

    // Search before serialization
    SearchParams search_params;
    search_params.n_probe = 3;
    auto results_before = index1.search(vectors[0], 10, search_params);

    // Serialize
    std::ostringstream oss;
    EXPECT_EQ(index1.serialize(oss), ErrorCode::Ok);

    // Deserialize
    IVFIndex index2(64, DistanceMetric::L2, params);
    std::istringstream iss(oss.str());
    EXPECT_EQ(index2.deserialize(iss), ErrorCode::Ok);

    // Search after deserialization - should get identical results
    auto results_after = index2.search(vectors[0], 10, search_params);

    ASSERT_EQ(results_before.size(), results_after.size());
    for (std::size_t i = 0; i < results_before.size(); ++i) {
        EXPECT_EQ(results_before[i].id, results_after[i].id);
        EXPECT_NEAR(results_before[i].distance, results_after[i].distance, 1e-6f);
    }
}

TEST(IVFIndexTest, DeserializeDimensionMismatch) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index1(8, DistanceMetric::L2, params);

    // Build with 8 dimensions
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors_ivf(50, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }
    index1.build(records);

    std::ostringstream oss;
    index1.serialize(oss);

    // Try to deserialize into index with different dimension
    IVFIndex index2(16, DistanceMetric::L2, params);
    std::istringstream iss(oss.str());
    EXPECT_EQ(index2.deserialize(iss), ErrorCode::DimensionMismatch);
}

TEST(IVFIndexTest, DeserializeInvalidMagic) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::string bad_data = "BAD!";
    std::istringstream iss(bad_data);
    EXPECT_EQ(index.deserialize(iss), ErrorCode::IOError);
}

TEST(IVFIndexTest, SerializePreservesAllVectors) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index1(8, DistanceMetric::L2, params);

    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors_ivf(50, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }
    index1.build(records);

    // Serialize and deserialize
    std::ostringstream oss;
    index1.serialize(oss);

    IVFIndex index2(8, DistanceMetric::L2, params);
    std::istringstream iss(oss.str());
    index2.deserialize(iss);

    // Verify all vectors are present
    for (std::size_t i = 0; i < 50; ++i) {
        EXPECT_TRUE(index2.contains(i));
    }
}

// ============================================================================
// Integration Tests (Ticket #2004)
// ============================================================================

TEST(IVFIndexTest, FullWorkflowBuildAddSearchRemoveSave) {
    IVFParams params;
    params.n_clusters = 5;

    IVFIndex index(64, DistanceMetric::L2, params);

    // Build with initial dataset
    std::vector<VectorRecord> records;
    auto vectors = generate_random_vectors_ivf(500, 64, 42);
    for (std::size_t i = 0; i < 500; ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }
    EXPECT_EQ(index.build(records), ErrorCode::Ok);

    // Add more vectors
    auto new_vectors = generate_random_vectors_ivf(100, 64, 99);
    for (std::size_t i = 0; i < 100; ++i) {
        EXPECT_EQ(index.add(500 + i, new_vectors[i]), ErrorCode::Ok);
    }
    EXPECT_EQ(index.size(), 600);

    // Search
    SearchParams search_params;
    search_params.n_probe = 3;
    auto results1 = index.search(vectors[0], 10, search_params);
    EXPECT_EQ(results1.size(), 10);

    // Remove some vectors
    for (std::size_t i = 100; i < 110; ++i) {
        EXPECT_EQ(index.remove(i), ErrorCode::Ok);
    }
    EXPECT_EQ(index.size(), 590);

    // Search again
    auto results2 = index.search(vectors[0], 10, search_params);

    // Save
    std::ostringstream oss;
    EXPECT_EQ(index.serialize(oss), ErrorCode::Ok);

    // Load
    IVFIndex index2(64, DistanceMetric::L2, params);
    std::istringstream iss(oss.str());
    EXPECT_EQ(index2.deserialize(iss), ErrorCode::Ok);

    // Search after load - should match results2
    auto results3 = index2.search(vectors[0], 10, search_params);
    ASSERT_EQ(results2.size(), results3.size());
    for (std::size_t i = 0; i < results2.size(); ++i) {
        EXPECT_EQ(results2[i].id, results3[i].id);
        EXPECT_NEAR(results2[i].distance, results3[i].distance, 1e-5f);
    }
}

// ============================================================================
// Distance Metric Tests
// ============================================================================

TEST(IVFIndexTest, AddWithL2Metric) {
    IVFParams params;
    params.n_clusters = 2;

    IVFIndex index(4, DistanceMetric::L2, params);

    // Two centroids at [0,0,0,0] and [10,0,0,0]
    std::vector<std::vector<float>> centroids = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {10.0f, 0.0f, 0.0f, 0.0f}
    };
    index.set_centroids(centroids);

    // Vector close to first centroid
    std::vector<float> vec1 = {0.1f, 0.0f, 0.0f, 0.0f};
    EXPECT_EQ(index.add(1, vec1), ErrorCode::Ok);

    // Vector close to second centroid
    std::vector<float> vec2 = {9.9f, 0.0f, 0.0f, 0.0f};
    EXPECT_EQ(index.add(2, vec2), ErrorCode::Ok);

    EXPECT_EQ(index.size(), 2);
}

TEST(IVFIndexTest, AddWithCosineMetric) {
    IVFParams params;
    params.n_clusters = 2;

    IVFIndex index(4, DistanceMetric::Cosine, params);

    // Two centroids in different directions
    std::vector<std::vector<float>> centroids = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f}
    };
    index.set_centroids(centroids);

    // Vector similar to first centroid direction
    std::vector<float> vec1 = {0.9f, 0.1f, 0.0f, 0.0f};
    EXPECT_EQ(index.add(1, vec1), ErrorCode::Ok);

    // Vector similar to second centroid direction
    std::vector<float> vec2 = {0.1f, 0.9f, 0.0f, 0.0f};
    EXPECT_EQ(index.add(2, vec2), ErrorCode::Ok);

    EXPECT_EQ(index.size(), 2);
}

TEST(IVFIndexTest, AddWithDotProductMetric) {
    IVFParams params;
    params.n_clusters = 2;

    IVFIndex index(4, DistanceMetric::DotProduct, params);

    std::vector<std::vector<float>> centroids = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f, 0.0f}
    };
    index.set_centroids(centroids);

    std::vector<float> vec = {0.5f, 0.5f, 0.0f, 0.0f};
    EXPECT_EQ(index.add(1, vec), ErrorCode::Ok);

    EXPECT_EQ(index.size(), 1);
}

// ============================================================================
// Thread Safety Tests (Basic)
// ============================================================================

TEST(IVFIndexTest, ConcurrentReads) {
    IVFParams params;
    params.n_clusters = 5;

    IVFIndex index(64, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(5, 64);
    index.set_centroids(centroids);

    // Add some vectors
    auto vectors = generate_random_vectors_ivf(100, 64);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    // Concurrent reads should not crash
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&index]() {
            for (int i = 0; i < 100; ++i) {
                index.contains(static_cast<std::uint64_t>(i));
                index.size();
                index.dimension();
                index.memory_usage();
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
}

// ============================================================================
// Various Dimensions Tests
// ============================================================================

TEST(IVFIndexTest, VariousDimensions) {
    IVFParams params;
    params.n_clusters = 5;

    std::vector<std::size_t> dimensions = {2, 8, 64, 128, 512};

    for (std::size_t dim : dimensions) {
        IVFIndex index(dim, DistanceMetric::L2, params);

        auto centroids = generate_test_centroids(5, dim);
        EXPECT_EQ(index.set_centroids(centroids), ErrorCode::Ok);

        auto vectors = generate_random_vectors_ivf(20, dim);
        for (std::size_t i = 0; i < vectors.size(); ++i) {
            EXPECT_EQ(index.add(i, vectors[i]), ErrorCode::Ok);
        }

        EXPECT_EQ(index.size(), 20);
        EXPECT_EQ(index.dimension(), dim);
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(IVFIndexTest, SingleCluster) {
    IVFParams params;
    params.n_clusters = 1;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::vector<std::vector<float>> centroids = {
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}
    };
    EXPECT_EQ(index.set_centroids(centroids), ErrorCode::Ok);

    // All vectors should go to the single cluster
    auto vectors = generate_random_vectors_ivf(50, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        EXPECT_EQ(index.add(i, vectors[i]), ErrorCode::Ok);
    }

    EXPECT_EQ(index.size(), 50);
}

TEST(IVFIndexTest, ManyClusters) {
    IVFParams params;
    params.n_clusters = 100;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(100, 8, 1.0f);
    EXPECT_EQ(index.set_centroids(centroids), ErrorCode::Ok);

    auto vectors = generate_random_vectors_ivf(200, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        EXPECT_EQ(index.add(i, vectors[i]), ErrorCode::Ok);
    }

    EXPECT_EQ(index.size(), 200);
}

TEST(IVFIndexTest, LargeIds) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    std::vector<float> vec(8, 1.0f);

    // Use large IDs
    EXPECT_EQ(index.add(1000000000ULL, vec), ErrorCode::Ok);
    EXPECT_EQ(index.add(std::numeric_limits<std::uint64_t>::max() - 1, vec), ErrorCode::Ok);

    EXPECT_TRUE(index.contains(1000000000ULL));
    EXPECT_TRUE(index.contains(std::numeric_limits<std::uint64_t>::max() - 1));
    EXPECT_EQ(index.size(), 2);
}
