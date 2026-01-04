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

// ============================================================================
// SIMD Support Detection
// ============================================================================
// Detect available SIMD instruction sets at compile time

#if defined(__AVX2__)
    #define LYNX_USE_AVX2 1
    #include <immintrin.h>
#elif defined(__AVX__)
    #define LYNX_USE_AVX 1
    #include <immintrin.h>
#endif

#if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
    #define LYNX_USE_SSE 1
    #ifndef LYNX_USE_AVX2
        #ifndef LYNX_USE_AVX
            #include <xmmintrin.h>
            #include <emmintrin.h>
        #endif
    #endif
#endif

namespace lynx {
namespace utils {

// ============================================================================
// Distance Metric Implementations
// ============================================================================

#if defined(LYNX_USE_AVX2) || defined(LYNX_USE_AVX)

// AVX/AVX2 implementation - processes 8 floats at a time
float calculate_l2_squared(std::span<const float> a, std::span<const float> b) {
    // Verify dimensions match
    if (a.size() != b.size()) {
        return -1.0f; // Error indicator
    }

    const std::size_t n = a.size();
    const float* ptr_a = a.data();
    const float* ptr_b = b.data();

    __m256 sum_vec = _mm256_setzero_ps();

    // Process 8 floats at a time with AVX
    std::size_t i = 0;
    const std::size_t simd_end = n - (n % 8);

    for (; i < simd_end; i += 8) {
        __m256 va = _mm256_loadu_ps(ptr_a + i);
        __m256 vb = _mm256_loadu_ps(ptr_b + i);
        __m256 diff = _mm256_sub_ps(va, vb);
        __m256 sq = _mm256_mul_ps(diff, diff);
        sum_vec = _mm256_add_ps(sum_vec, sq);
    }

    // Horizontal sum of the 8 floats in sum_vec
    // sum_vec = [s0, s1, s2, s3, s4, s5, s6, s7]
    __m128 low = _mm256_castps256_ps128(sum_vec);       // [s0, s1, s2, s3]
    __m128 high = _mm256_extractf128_ps(sum_vec, 1);    // [s4, s5, s6, s7]
    __m128 sum128 = _mm_add_ps(low, high);              // [s0+s4, s1+s5, s2+s6, s3+s7]

    // Horizontal sum of 4 floats (SSE2 compatible)
    __m128 shuf = _mm_shuffle_ps(sum128, sum128, _MM_SHUFFLE(2, 3, 0, 1)); // [s1, s0, s3, s2]
    __m128 sums = _mm_add_ps(sum128, shuf);             // [s0+s1, s1+s0, s2+s3, s3+s2]
    shuf = _mm_movehl_ps(shuf, sums);                   // Move high half to low
    sums = _mm_add_ss(sums, shuf);                      // Final sum in lowest element

    float sum = _mm_cvtss_f32(sums);

    // Handle remaining elements (scalar)
    for (; i < n; ++i) {
        const float diff = ptr_a[i] - ptr_b[i];
        sum += diff * diff;
    }

    return sum;
}

#elif defined(LYNX_USE_SSE)

// SSE implementation - processes 4 floats at a time
float calculate_l2_squared(std::span<const float> a, std::span<const float> b) {
    // Verify dimensions match
    if (a.size() != b.size()) {
        return -1.0f; // Error indicator
    }

    const std::size_t n = a.size();
    const float* ptr_a = a.data();
    const float* ptr_b = b.data();

    __m128 sum_vec = _mm_setzero_ps();

    // Process 4 floats at a time with SSE
    std::size_t i = 0;
    const std::size_t simd_end = n - (n % 4);

    for (; i < simd_end; i += 4) {
        __m128 va = _mm_loadu_ps(ptr_a + i);
        __m128 vb = _mm_loadu_ps(ptr_b + i);
        __m128 diff = _mm_sub_ps(va, vb);
        __m128 sq = _mm_mul_ps(diff, diff);
        sum_vec = _mm_add_ps(sum_vec, sq);
    }

    // Horizontal sum of 4 floats (SSE2 compatible)
    // sum_vec = [s0, s1, s2, s3]
    __m128 shuf = _mm_shuffle_ps(sum_vec, sum_vec, _MM_SHUFFLE(2, 3, 0, 1)); // [s1, s0, s3, s2]
    __m128 sums = _mm_add_ps(sum_vec, shuf);   // [s0+s1, s1+s0, s2+s3, s3+s2]
    shuf = _mm_movehl_ps(shuf, sums);          // Move high half to low
    sums = _mm_add_ss(sums, shuf);             // Final sum in lowest element

    float sum = _mm_cvtss_f32(sums);

    // Handle remaining elements (scalar)
    for (; i < n; ++i) {
        const float diff = ptr_a[i] - ptr_b[i];
        sum += diff * diff;
    }

    return sum;
}

#else

// Scalar fallback implementation
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

#endif

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
