/**
 * @file utils.cpp
 * @brief Utility function implementations for Lynx vector database
 *
 * This file contains the implementations of utility functions used across
 * the Lynx codebase, including distance metric calculations.
 *
 * @copyright MIT License
 */

#include "utils.h"
#include <cmath>
#include <algorithm>

namespace lynx {
namespace utils {

// ============================================================================
// Distance Metric Implementations
// ============================================================================

float calculate_l2_squared(std::span<const float> a, std::span<const float> b) {
    // Verify dimensions match
    if (a.size() != b.size()) {
        return -1.0f; // Error indicator
    }

    float sum = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sum;
}

float calculate_l2(std::span<const float> a, std::span<const float> b) {
    const float squared = calculate_l2_squared(a, b);
    if (squared < 0.0f) {
        return -1.0f; // Error indicator (dimension mismatch)
    }
    return std::sqrt(squared);
}

float calculate_cosine(std::span<const float> a, std::span<const float> b) {
    // Verify dimensions match
    if (a.size() != b.size()) {
        return -1.0f; // Error indicator
    }

    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (std::size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);

    // Handle zero vectors (avoid division by zero)
    if (norm_a < 1e-10f || norm_b < 1e-10f) {
        return 1.0f; // Maximum dissimilarity for zero vectors
    }

    // Cosine similarity: dot(a,b) / (|a| * |b|)
    const float cosine_similarity = dot_product / (norm_a * norm_b);

    // Clamp to [-1, 1] to handle floating point errors
    const float clamped = std::clamp(cosine_similarity, -1.0f, 1.0f);

    // Return cosine distance: 1 - cosine_similarity
    // Range is [0, 2]: 0 for identical, 1 for orthogonal, 2 for opposite
    return 1.0f - clamped;
}

float calculate_dot_product(std::span<const float> a, std::span<const float> b) {
    // Verify dimensions match
    if (a.size() != b.size()) {
        return -1.0f; // Error indicator
    }

    float dot_product = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
    }

    // Return negative dot product (so smaller means more similar)
    return -dot_product;
}

float calculate_distance(
    std::span<const float> a,
    std::span<const float> b,
    DistanceMetric metric) {

    switch (metric) {
        case DistanceMetric::L2:
            return calculate_l2(a, b);
        case DistanceMetric::Cosine:
            return calculate_cosine(a, b);
        case DistanceMetric::DotProduct:
            return calculate_dot_product(a, b);
        default:
            return -1.0f; // Error indicator for unknown metric
    }
}

} // namespace utils
} // namespace lynx
