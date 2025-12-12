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
// Search Stub Tests (To be properly tested in #2003)
// ============================================================================

TEST(IVFIndexTest, SearchReturnsEmptyForNow) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    auto centroids = generate_test_centroids(3, 8);
    index.set_centroids(centroids);

    std::vector<float> query(8, 0.0f);
    SearchParams search_params;

    auto results = index.search(query, 10, search_params);

    // Search returns empty for now (to be implemented in #2003)
    EXPECT_TRUE(results.empty());
}

TEST(IVFIndexTest, SearchDimensionMismatch) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::vector<float> query(16, 0.0f);  // Wrong dimension
    SearchParams search_params;

    auto results = index.search(query, 10, search_params);
    EXPECT_TRUE(results.empty());
}

// ============================================================================
// Stub Method Tests (To be implemented in later tickets)
// ============================================================================

TEST(IVFIndexTest, RemoveNotImplemented) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    EXPECT_EQ(index.remove(1), ErrorCode::NotImplemented);
}

TEST(IVFIndexTest, BuildNotImplemented) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::vector<VectorRecord> records;
    EXPECT_EQ(index.build(records), ErrorCode::NotImplemented);
}

TEST(IVFIndexTest, SerializeNotImplemented) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::ostringstream oss;
    EXPECT_EQ(index.serialize(oss), ErrorCode::NotImplemented);
}

TEST(IVFIndexTest, DeserializeNotImplemented) {
    IVFParams params;
    params.n_clusters = 3;

    IVFIndex index(8, DistanceMetric::L2, params);

    std::istringstream iss;
    EXPECT_EQ(index.deserialize(iss), ErrorCode::NotImplemented);
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
