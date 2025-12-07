/**
 * @file test_distance_metrics.cpp
 * @brief Unit tests for Lynx distance metric functions
 */

#include <gtest/gtest.h>
#include "lynx/lynx.h"
#include <cmath>
#include <vector>

// ============================================================================
// L2 Distance Tests
// ============================================================================

TEST(DistanceMetricsTest, L2IdenticalVectors) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    float distance = lynx::distance_l2(a, a);
    EXPECT_FLOAT_EQ(distance, 0.0f);
}

TEST(DistanceMetricsTest, L2SimpleCase) {
    std::vector<float> a = {0.0f, 0.0f, 0.0f};
    std::vector<float> b = {3.0f, 4.0f, 0.0f};
    float distance = lynx::distance_l2(a, b);
    EXPECT_FLOAT_EQ(distance, 5.0f); // 3-4-5 triangle
}

TEST(DistanceMetricsTest, L2Symmetric) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {4.0f, 5.0f, 6.0f};
    float dist_ab = lynx::distance_l2(a, b);
    float dist_ba = lynx::distance_l2(b, a);
    EXPECT_FLOAT_EQ(dist_ab, dist_ba);
}

TEST(DistanceMetricsTest, L2HighDimensional) {
    std::vector<float> a(128, 1.0f);
    std::vector<float> b(128, 2.0f);
    float distance = lynx::distance_l2(a, b);
    // sqrt(128 * (2-1)^2) = sqrt(128) ≈ 11.314
    EXPECT_NEAR(distance, std::sqrt(128.0f), 1e-5f);
}

TEST(DistanceMetricsTest, L2DimensionMismatch) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {1.0f, 2.0f};
    float distance = lynx::distance_l2(a, b);
    EXPECT_LT(distance, 0.0f); // Should return error indicator
}

TEST(DistanceMetricsTest, L2EmptyVectors) {
    std::vector<float> a;
    std::vector<float> b;
    float distance = lynx::distance_l2(a, b);
    EXPECT_FLOAT_EQ(distance, 0.0f);
}

// ============================================================================
// L2 Squared Distance Tests
// ============================================================================

TEST(DistanceMetricsTest, L2SquaredIdenticalVectors) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    float distance = lynx::distance_l2_squared(a, a);
    EXPECT_FLOAT_EQ(distance, 0.0f);
}

TEST(DistanceMetricsTest, L2SquaredSimpleCase) {
    std::vector<float> a = {0.0f, 0.0f, 0.0f};
    std::vector<float> b = {3.0f, 4.0f, 0.0f};
    float distance = lynx::distance_l2_squared(a, b);
    EXPECT_FLOAT_EQ(distance, 25.0f); // 3^2 + 4^2 = 25
}

TEST(DistanceMetricsTest, L2SquaredConsistentWithL2) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> b = {5.0f, 6.0f, 7.0f, 8.0f};
    float l2 = lynx::distance_l2(a, b);
    float l2_squared = lynx::distance_l2_squared(a, b);
    EXPECT_FLOAT_EQ(l2 * l2, l2_squared);
}

TEST(DistanceMetricsTest, L2SquaredDimensionMismatch) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {1.0f, 2.0f};
    float distance = lynx::distance_l2_squared(a, b);
    EXPECT_LT(distance, 0.0f); // Should return error indicator
}

// ============================================================================
// Cosine Distance Tests
// ============================================================================

TEST(DistanceMetricsTest, CosineIdenticalVectors) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    float distance = lynx::distance_cosine(a, a);
    EXPECT_NEAR(distance, 0.0f, 1e-6f);
}

TEST(DistanceMetricsTest, CosineOrthogonalVectors) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f, 0.0f};
    float distance = lynx::distance_cosine(a, b);
    EXPECT_NEAR(distance, 1.0f, 1e-6f); // 90 degrees => cos(90°) = 0 => distance = 1
}

TEST(DistanceMetricsTest, CosineOppositeVectors) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {-1.0f, 0.0f, 0.0f};
    float distance = lynx::distance_cosine(a, b);
    EXPECT_NEAR(distance, 2.0f, 1e-6f); // 180 degrees => cos(180°) = -1 => distance = 2
}

TEST(DistanceMetricsTest, CosineScaleInvariant) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {2.0f, 4.0f, 6.0f}; // Same direction, different magnitude
    float distance = lynx::distance_cosine(a, b);
    EXPECT_NEAR(distance, 0.0f, 1e-6f);
}

TEST(DistanceMetricsTest, CosineSymmetric) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {4.0f, 5.0f, 6.0f};
    float dist_ab = lynx::distance_cosine(a, b);
    float dist_ba = lynx::distance_cosine(b, a);
    EXPECT_FLOAT_EQ(dist_ab, dist_ba);
}

TEST(DistanceMetricsTest, CosineNormalizedVectors) {
    // Pre-normalized vectors (unit length)
    std::vector<float> a = {1.0f / std::sqrt(3.0f), 1.0f / std::sqrt(3.0f), 1.0f / std::sqrt(3.0f)};
    std::vector<float> b = {1.0f, 0.0f, 0.0f};
    float distance = lynx::distance_cosine(a, b);
    // cos(angle) = 1/sqrt(3) => distance = 1 - 1/sqrt(3) ≈ 0.4226
    EXPECT_NEAR(distance, 1.0f - 1.0f / std::sqrt(3.0f), 1e-5f);
}

TEST(DistanceMetricsTest, CosineZeroVector) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {0.0f, 0.0f, 0.0f};
    float distance = lynx::distance_cosine(a, b);
    EXPECT_NEAR(distance, 1.0f, 1e-6f); // Maximum dissimilarity
}

TEST(DistanceMetricsTest, CosineBothZeroVectors) {
    std::vector<float> a = {0.0f, 0.0f, 0.0f};
    std::vector<float> b = {0.0f, 0.0f, 0.0f};
    float distance = lynx::distance_cosine(a, b);
    EXPECT_NEAR(distance, 1.0f, 1e-6f); // Maximum dissimilarity
}

TEST(DistanceMetricsTest, CosineDimensionMismatch) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {1.0f, 2.0f};
    float distance = lynx::distance_cosine(a, b);
    EXPECT_LT(distance, 0.0f); // Should return error indicator
}

TEST(DistanceMetricsTest, CosineHighDimensional) {
    std::vector<float> a(128, 1.0f);
    std::vector<float> b(128, 1.0f);
    float distance = lynx::distance_cosine(a, b);
    EXPECT_NEAR(distance, 0.0f, 1e-5f);
}

// ============================================================================
// Dot Product Distance Tests
// ============================================================================

TEST(DistanceMetricsTest, DotProductIdenticalVectors) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    float distance = lynx::distance_dot_product(a, a);
    // dot(a, a) = 1 + 4 + 9 = 14, return -14
    EXPECT_FLOAT_EQ(distance, -14.0f);
}

TEST(DistanceMetricsTest, DotProductOrthogonalVectors) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f, 0.0f};
    float distance = lynx::distance_dot_product(a, b);
    EXPECT_FLOAT_EQ(distance, 0.0f); // Orthogonal vectors have dot product 0
}

TEST(DistanceMetricsTest, DotProductSimpleCase) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {4.0f, 5.0f, 6.0f};
    float distance = lynx::distance_dot_product(a, b);
    // dot(a, b) = 4 + 10 + 18 = 32, return -32
    EXPECT_FLOAT_EQ(distance, -32.0f);
}

TEST(DistanceMetricsTest, DotProductSymmetric) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {4.0f, 5.0f, 6.0f};
    float dist_ab = lynx::distance_dot_product(a, b);
    float dist_ba = lynx::distance_dot_product(b, a);
    EXPECT_FLOAT_EQ(dist_ab, dist_ba);
}

TEST(DistanceMetricsTest, DotProductNormalizedVectors) {
    // For normalized vectors, dot product = cosine similarity
    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f, 0.0f};
    float distance = lynx::distance_dot_product(a, b);
    EXPECT_FLOAT_EQ(distance, 0.0f); // Orthogonal normalized vectors
}

TEST(DistanceMetricsTest, DotProductNegativeValues) {
    std::vector<float> a = {1.0f, -2.0f, 3.0f};
    std::vector<float> b = {-1.0f, 2.0f, -3.0f};
    float distance = lynx::distance_dot_product(a, b);
    // dot(a, b) = -1 + (-4) + (-9) = -14, return -(-14) = 14
    EXPECT_FLOAT_EQ(distance, 14.0f);
}

TEST(DistanceMetricsTest, DotProductDimensionMismatch) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {1.0f, 2.0f};
    float distance = lynx::distance_dot_product(a, b);
    EXPECT_LT(distance, 0.0f); // Should return error indicator
}

TEST(DistanceMetricsTest, DotProductHighDimensional) {
    std::vector<float> a(128, 1.0f);
    std::vector<float> b(128, 2.0f);
    float distance = lynx::distance_dot_product(a, b);
    // dot(a, b) = 128 * 1 * 2 = 256, return -256
    EXPECT_FLOAT_EQ(distance, -256.0f);
}

// ============================================================================
// calculate_distance() Tests
// ============================================================================

TEST(DistanceMetricsTest, CalculateDistanceL2) {
    std::vector<float> a = {0.0f, 0.0f, 0.0f};
    std::vector<float> b = {3.0f, 4.0f, 0.0f};
    float distance = lynx::calculate_distance(a, b, lynx::DistanceMetric::L2);
    EXPECT_FLOAT_EQ(distance, 5.0f);
}

TEST(DistanceMetricsTest, CalculateDistanceCosine) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f, 0.0f};
    float distance = lynx::calculate_distance(a, b, lynx::DistanceMetric::Cosine);
    EXPECT_NEAR(distance, 1.0f, 1e-6f);
}

TEST(DistanceMetricsTest, CalculateDistanceDotProduct) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {4.0f, 5.0f, 6.0f};
    float distance = lynx::calculate_distance(a, b, lynx::DistanceMetric::DotProduct);
    EXPECT_FLOAT_EQ(distance, -32.0f);
}

TEST(DistanceMetricsTest, CalculateDistanceAllMetricsConsistent) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {4.0f, 5.0f, 6.0f};

    // Test that calculate_distance matches direct function calls
    float l2_direct = lynx::distance_l2(a, b);
    float l2_calc = lynx::calculate_distance(a, b, lynx::DistanceMetric::L2);
    EXPECT_FLOAT_EQ(l2_direct, l2_calc);

    float cosine_direct = lynx::distance_cosine(a, b);
    float cosine_calc = lynx::calculate_distance(a, b, lynx::DistanceMetric::Cosine);
    EXPECT_FLOAT_EQ(cosine_direct, cosine_calc);

    float dot_direct = lynx::distance_dot_product(a, b);
    float dot_calc = lynx::calculate_distance(a, b, lynx::DistanceMetric::DotProduct);
    EXPECT_FLOAT_EQ(dot_direct, dot_calc);
}

// ============================================================================
// Edge Cases and Robustness Tests
// ============================================================================

TEST(DistanceMetricsTest, SingleDimensionVectors) {
    std::vector<float> a = {5.0f};
    std::vector<float> b = {3.0f};

    float l2 = lynx::distance_l2(a, b);
    EXPECT_FLOAT_EQ(l2, 2.0f);

    float dot = lynx::distance_dot_product(a, b);
    EXPECT_FLOAT_EQ(dot, -15.0f);
}

TEST(DistanceMetricsTest, LargeVectorValues) {
    std::vector<float> a = {1000.0f, 2000.0f, 3000.0f};
    std::vector<float> b = {1000.0f, 2000.0f, 3000.0f};

    float distance = lynx::distance_l2(a, b);
    EXPECT_FLOAT_EQ(distance, 0.0f);

    float cosine = lynx::distance_cosine(a, b);
    EXPECT_NEAR(cosine, 0.0f, 1e-5f);
}

TEST(DistanceMetricsTest, SmallVectorValues) {
    std::vector<float> a = {0.001f, 0.002f, 0.003f};
    std::vector<float> b = {0.001f, 0.002f, 0.003f};

    float distance = lynx::distance_l2(a, b);
    EXPECT_NEAR(distance, 0.0f, 1e-7f);

    float cosine = lynx::distance_cosine(a, b);
    EXPECT_NEAR(cosine, 0.0f, 1e-6f);
}

TEST(DistanceMetricsTest, MixedPositiveNegativeValues) {
    std::vector<float> a = {-1.0f, 2.0f, -3.0f, 4.0f};
    std::vector<float> b = {1.0f, -2.0f, 3.0f, -4.0f};

    float l2 = lynx::distance_l2(a, b);
    // (-1-1)^2 + (2-(-2))^2 + (-3-3)^2 + (4-(-4))^2 = 4 + 16 + 36 + 64 = 120
    EXPECT_NEAR(l2, std::sqrt(120.0f), 1e-5f);

    float dot = lynx::distance_dot_product(a, b);
    // -1*1 + 2*(-2) + (-3)*3 + 4*(-4) = -1 -4 -9 -16 = -30, return 30
    EXPECT_FLOAT_EQ(dot, 30.0f);
}
