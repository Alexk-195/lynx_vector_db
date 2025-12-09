/**
 * @file utils.h
 * @brief Utility functions for Lynx vector database
 *
 * This header provides internal utility functions used across the Lynx codebase,
 * including optimized distance metric calculations.
 *
 * @copyright MIT License
 */

#ifndef LYNX_UTILS_H
#define LYNX_UTILS_H

#include "lynx.h"
#include <span>
#include <cstddef>

namespace lynx {
namespace utils {

// ============================================================================
// Distance Metric Implementations
// ============================================================================

/**
 * @brief Calculate squared L2 distance between two vectors.
 *
 * Computes: sum((a[i] - b[i])^2)
 * This is faster than L2 distance as it avoids the sqrt operation.
 * Useful when only relative distances matter.
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @return Squared L2 distance, or -1.0f on dimension mismatch
 */
[[nodiscard]] float calculate_l2_squared(std::span<const float> a, std::span<const float> b);

/**
 * @brief Calculate L2 (Euclidean) distance between two vectors.
 *
 * Computes: sqrt(sum((a[i] - b[i])^2))
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @return L2 distance, or -1.0f on dimension mismatch
 */
[[nodiscard]] float calculate_l2(std::span<const float> a, std::span<const float> b);

/**
 * @brief Calculate cosine distance between two vectors.
 *
 * Computes: 1 - (a·b) / (|a| * |b|)
 * Returns 0 for identical directions, 2 for opposite directions.
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @return Cosine distance in range [0, 2], or -1.0f on dimension mismatch
 */
[[nodiscard]] float calculate_cosine(std::span<const float> a, std::span<const float> b);

/**
 * @brief Calculate negative dot product between two vectors.
 *
 * Computes: -(a·b)
 * Negative is used so that "smaller is more similar" (consistent with other metrics).
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @return Negative dot product, or -1.0f on dimension mismatch
 */
[[nodiscard]] float calculate_dot_product(std::span<const float> a, std::span<const float> b);

/**
 * @brief Calculate distance using the specified metric.
 *
 * Dispatches to the appropriate distance function based on metric type.
 *
 * @param a First vector
 * @param b Second vector (must have same length as a)
 * @param metric Distance metric to use
 * @return Distance value (interpretation depends on metric)
 */
[[nodiscard]] float calculate_distance(
    std::span<const float> a,
    std::span<const float> b,
    DistanceMetric metric);

} // namespace utils
} // namespace lynx

#endif // LYNX_UTILS_H
