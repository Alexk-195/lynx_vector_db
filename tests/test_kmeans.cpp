/**
 * @file test_kmeans.cpp
 * @brief Unit tests for K-Means clustering algorithm
 *
 * @copyright MIT License
 */

#include "../src/lib/kmeans.h"
#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

using namespace lynx;
using namespace lynx::clustering;

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
 * @brief Generate clustered data (Gaussian blobs).
 */
std::vector<std::vector<float>> generate_clustered_data(
    std::size_t vectors_per_cluster, std::size_t num_clusters,
    std::size_t dimension, float separation = 5.0f, std::uint64_t seed = 42) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.0f, 0.5f);

    std::vector<std::vector<float>> vectors;
    vectors.reserve(vectors_per_cluster * num_clusters);

    for (std::size_t cluster = 0; cluster < num_clusters; ++cluster) {
        // Cluster center
        std::vector<float> center(dimension, 0.0f);
        center[0] = static_cast<float>(cluster) * separation;

        // Generate vectors around center
        for (std::size_t i = 0; i < vectors_per_cluster; ++i) {
            std::vector<float> vec(dimension);
            for (std::size_t d = 0; d < dimension; ++d) {
                vec[d] = center[d] + dist(rng);
            }
            vectors.push_back(std::move(vec));
        }
    }

    return vectors;
}

/**
 * @brief Calculate clustering purity (quality metric).
 */
float calculate_purity(const std::vector<std::size_t>& assignments,
                       const std::vector<std::size_t>& ground_truth) {
    if (assignments.size() != ground_truth.size()) {
        return 0.0f;
    }

    std::size_t num_clusters = *std::max_element(assignments.begin(), assignments.end()) + 1;
    std::size_t num_classes = *std::max_element(ground_truth.begin(), ground_truth.end()) + 1;

    // Count matrix: cluster x class
    std::vector<std::vector<std::size_t>> counts(num_clusters, std::vector<std::size_t>(num_classes, 0));

    for (std::size_t i = 0; i < assignments.size(); ++i) {
        counts[assignments[i]][ground_truth[i]]++;
    }

    // Sum of max counts per cluster
    std::size_t correct = 0;
    for (std::size_t c = 0; c < num_clusters; ++c) {
        correct += *std::max_element(counts[c].begin(), counts[c].end());
    }

    return static_cast<float>(correct) / static_cast<float>(assignments.size());
}

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST(KMeansTest, ConstructorValid) {
    KMeansParams params;
    params.random_seed = 42;

    EXPECT_NO_THROW({
        KMeans kmeans(3, 64, DistanceMetric::L2, params);
        EXPECT_EQ(kmeans.k(), 3);
        EXPECT_EQ(kmeans.dimension(), 64);
        EXPECT_FALSE(kmeans.is_fitted());
    });
}

TEST(KMeansTest, ConstructorInvalid) {
    KMeansParams params;

    // k = 0 should throw
    EXPECT_THROW({
        KMeans kmeans(0, 64, DistanceMetric::L2, params);
    }, std::invalid_argument);

    // dimension = 0 should throw
    EXPECT_THROW({
        KMeans kmeans(3, 0, DistanceMetric::L2, params);
    }, std::invalid_argument);
}

TEST(KMeansTest, FitBasic) {
    KMeansParams params;
    params.random_seed = 42;
    params.max_iterations = 100;

    KMeans kmeans(3, 2, DistanceMetric::L2, params);

    // Generate simple 2D data with 3 well-separated clusters
    auto vectors = generate_clustered_data(30, 3, 2, 10.0f, 42);

    EXPECT_NO_THROW({
        kmeans.fit(vectors);
    });

    EXPECT_TRUE(kmeans.is_fitted());
    EXPECT_EQ(kmeans.centroids().size(), 3);
    EXPECT_EQ(kmeans.centroids()[0].size(), 2);
}

TEST(KMeansTest, FitEmptyVectors) {
    KMeansParams params;
    params.random_seed = 42;

    KMeans kmeans(3, 64, DistanceMetric::L2, params);
    std::vector<std::vector<float>> empty_vectors;

    EXPECT_THROW({
        kmeans.fit(empty_vectors);
    }, std::invalid_argument);
}

TEST(KMeansTest, FitDimensionMismatch) {
    KMeansParams params;
    params.random_seed = 42;

    KMeans kmeans(3, 64, DistanceMetric::L2, params);

    // Create vectors with wrong dimension
    std::vector<std::vector<float>> vectors;
    vectors.push_back(std::vector<float>(32, 1.0f));  // Wrong dimension
    vectors.push_back(std::vector<float>(64, 1.0f));  // Correct dimension

    EXPECT_THROW({
        kmeans.fit(vectors);
    }, std::invalid_argument);
}

TEST(KMeansTest, PredictBeforeFit) {
    KMeansParams params;
    params.random_seed = 42;

    KMeans kmeans(3, 64, DistanceMetric::L2, params);
    auto vectors = generate_random_vectors(10, 64, 42);

    EXPECT_THROW({
        kmeans.predict(vectors);
    }, std::logic_error);
}

TEST(KMeansTest, CentroidsBeforeFit) {
    KMeansParams params;
    params.random_seed = 42;

    KMeans kmeans(3, 64, DistanceMetric::L2, params);

    EXPECT_THROW({
        auto c = kmeans.centroids();
    }, std::logic_error);
}

// ============================================================================
// K-Means++ Initialization Tests
// ============================================================================

TEST(KMeansTest, KMeansPlusPlusInitialization) {
    KMeansParams params;
    params.random_seed = 42;
    params.max_iterations = 0;  // No iterations, just initialization

    KMeans kmeans(3, 2, DistanceMetric::L2, params);

    // Generate well-separated clusters
    auto vectors = generate_clustered_data(30, 3, 2, 10.0f, 42);
    kmeans.fit(vectors);

    // With k-means++, centroids should be relatively far apart
    const auto& centroids = kmeans.centroids();
    ASSERT_EQ(centroids.size(), 3);

    // Calculate distances between centroids
    for (std::size_t i = 0; i < centroids.size(); ++i) {
        for (std::size_t j = i + 1; j < centroids.size(); ++j) {
            float dist = 0.0f;
            for (std::size_t d = 0; d < centroids[i].size(); ++d) {
                float diff = centroids[i][d] - centroids[j][d];
                dist += diff * diff;
            }
            dist = std::sqrt(dist);

            // Centroids should be reasonably separated (at least 5.0 apart)
            EXPECT_GT(dist, 5.0f);
        }
    }
}

// ============================================================================
// Clustering Quality Tests
// ============================================================================

TEST(KMeansTest, ClusteringQualityL2) {
    KMeansParams params;
    params.random_seed = 42;
    params.max_iterations = 100;

    KMeans kmeans(3, 2, DistanceMetric::L2, params);

    // Generate 3 well-separated clusters (100 vectors each)
    auto vectors = generate_clustered_data(100, 3, 2, 10.0f, 42);

    // Ground truth labels
    std::vector<std::size_t> ground_truth;
    for (std::size_t c = 0; c < 3; ++c) {
        for (std::size_t i = 0; i < 100; ++i) {
            ground_truth.push_back(c);
        }
    }

    // Fit and predict
    kmeans.fit(vectors);
    auto assignments = kmeans.predict(vectors);

    // Calculate purity
    float purity = calculate_purity(assignments, ground_truth);

    // With well-separated clusters, purity should be > 0.95
    EXPECT_GT(purity, 0.95f);
}

TEST(KMeansTest, ClusteringQualityCosine) {
    KMeansParams params;
    params.random_seed = 42;
    params.max_iterations = 100;

    KMeans kmeans(3, 8, DistanceMetric::Cosine, params);

    // Generate clustered data
    auto vectors = generate_clustered_data(50, 3, 8, 5.0f, 42);

    // Fit
    kmeans.fit(vectors);
    EXPECT_TRUE(kmeans.is_fitted());

    // Predict
    auto assignments = kmeans.predict(vectors);
    EXPECT_EQ(assignments.size(), 150);

    // All assignments should be valid
    for (auto cluster : assignments) {
        EXPECT_LT(cluster, 3);
    }
}

TEST(KMeansTest, ClusteringQualityDotProduct) {
    KMeansParams params;
    params.random_seed = 42;
    params.max_iterations = 100;

    KMeans kmeans(3, 8, DistanceMetric::DotProduct, params);

    // Generate clustered data
    auto vectors = generate_clustered_data(50, 3, 8, 5.0f, 42);

    // Fit
    kmeans.fit(vectors);
    EXPECT_TRUE(kmeans.is_fitted());

    // Predict
    auto assignments = kmeans.predict(vectors);
    EXPECT_EQ(assignments.size(), 150);
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST(KMeansTest, EdgeCaseKGreaterThanN) {
    KMeansParams params;
    params.random_seed = 42;

    // k=10 but only 5 vectors
    KMeans kmeans(10, 64, DistanceMetric::L2, params);
    auto vectors = generate_random_vectors(5, 64, 42);

    // Should automatically reduce k to 5
    EXPECT_NO_THROW({
        kmeans.fit(vectors);
    });

    EXPECT_EQ(kmeans.k(), 5);  // k should be reduced
    EXPECT_EQ(kmeans.centroids().size(), 5);
}

TEST(KMeansTest, EdgeCaseKEqualsOne) {
    KMeansParams params;
    params.random_seed = 42;

    KMeans kmeans(1, 64, DistanceMetric::L2, params);
    auto vectors = generate_random_vectors(100, 64, 42);

    kmeans.fit(vectors);

    EXPECT_EQ(kmeans.centroids().size(), 1);

    // All vectors should be assigned to cluster 0
    auto assignments = kmeans.predict(vectors);
    for (auto cluster : assignments) {
        EXPECT_EQ(cluster, 0);
    }
}

TEST(KMeansTest, EdgeCaseKEqualsN) {
    KMeansParams params;
    params.random_seed = 42;

    // k = N = 10
    KMeans kmeans(10, 64, DistanceMetric::L2, params);
    auto vectors = generate_random_vectors(10, 64, 42);

    kmeans.fit(vectors);

    EXPECT_EQ(kmeans.centroids().size(), 10);
}

TEST(KMeansTest, EdgeCaseSingleVectorClusters) {
    KMeansParams params;
    params.random_seed = 42;

    // k=5, N=5 (each vector becomes its own cluster)
    KMeans kmeans(5, 2, DistanceMetric::L2, params);
    auto vectors = generate_random_vectors(5, 2, 42);

    kmeans.fit(vectors);

    EXPECT_EQ(kmeans.centroids().size(), 5);
}

// ============================================================================
// Convergence Tests
// ============================================================================

TEST(KMeansTest, ConvergenceThreshold) {
    KMeansParams params;
    params.random_seed = 42;
    params.max_iterations = 1000;
    params.convergence_threshold = 1e-6f;  // Very tight threshold

    KMeans kmeans(3, 2, DistanceMetric::L2, params);

    // Generate well-separated clusters
    auto vectors = generate_clustered_data(50, 3, 2, 10.0f, 42);

    // Should converge before max iterations
    kmeans.fit(vectors);

    EXPECT_TRUE(kmeans.is_fitted());
    EXPECT_EQ(kmeans.centroids().size(), 3);
}

// ============================================================================
// Reproducibility Tests
// ============================================================================

TEST(KMeansTest, ReproducibilityWithSeed) {
    KMeansParams params1;
    params1.random_seed = 42;
    params1.max_iterations = 50;

    KMeansParams params2;
    params2.random_seed = 42;
    params2.max_iterations = 50;

    KMeans kmeans1(3, 8, DistanceMetric::L2, params1);
    KMeans kmeans2(3, 8, DistanceMetric::L2, params2);

    auto vectors = generate_random_vectors(100, 8, 123);

    kmeans1.fit(vectors);
    kmeans2.fit(vectors);

    // With same seed, centroids should be identical
    const auto& centroids1 = kmeans1.centroids();
    const auto& centroids2 = kmeans2.centroids();

    ASSERT_EQ(centroids1.size(), centroids2.size());
    for (std::size_t c = 0; c < centroids1.size(); ++c) {
        ASSERT_EQ(centroids1[c].size(), centroids2[c].size());
        for (std::size_t d = 0; d < centroids1[c].size(); ++d) {
            EXPECT_FLOAT_EQ(centroids1[c][d], centroids2[c][d]);
        }
    }
}

// ============================================================================
// Different Dimensions Tests
// ============================================================================

TEST(KMeansTest, VariousDimensions) {
    KMeansParams params;
    params.random_seed = 42;

    std::vector<std::size_t> dimensions = {2, 8, 64, 128, 512};

    for (std::size_t dim : dimensions) {
        KMeans kmeans(5, dim, DistanceMetric::L2, params);
        auto vectors = generate_random_vectors(50, dim, 42);

        EXPECT_NO_THROW({
            kmeans.fit(vectors);
        });

        EXPECT_TRUE(kmeans.is_fitted());
        EXPECT_EQ(kmeans.centroids().size(), 5);
        EXPECT_EQ(kmeans.centroids()[0].size(), dim);
    }
}

// ============================================================================
// Predict Tests
// ============================================================================

TEST(KMeansTest, PredictAfterFit) {
    KMeansParams params;
    params.random_seed = 42;

    KMeans kmeans(3, 8, DistanceMetric::L2, params);

    // Fit on training data
    auto train_vectors = generate_random_vectors(100, 8, 42);
    kmeans.fit(train_vectors);

    // Predict on new data
    auto test_vectors = generate_random_vectors(50, 8, 999);
    auto assignments = kmeans.predict(test_vectors);

    EXPECT_EQ(assignments.size(), 50);

    // All assignments should be valid cluster IDs
    for (auto cluster : assignments) {
        EXPECT_LT(cluster, 3);
    }
}

TEST(KMeansTest, PredictDimensionMismatch) {
    KMeansParams params;
    params.random_seed = 42;

    KMeans kmeans(3, 64, DistanceMetric::L2, params);

    auto train_vectors = generate_random_vectors(100, 64, 42);
    kmeans.fit(train_vectors);

    // Try to predict with wrong dimension
    auto test_vectors = generate_random_vectors(10, 32, 999);  // Wrong dimension

    EXPECT_THROW({
        kmeans.predict(test_vectors);
    }, std::invalid_argument);
}
