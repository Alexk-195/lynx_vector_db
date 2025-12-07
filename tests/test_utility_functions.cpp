/**
 * @file test_utility_functions.cpp
 * @brief Unit tests for Lynx utility functions
 */

#include <gtest/gtest.h>
#include "lynx/lynx.h"

// ============================================================================
// Error String Tests
// ============================================================================

TEST(UtilityFunctionsTest, ErrorStringOk) {
    EXPECT_STREQ(lynx::error_string(lynx::ErrorCode::Ok), "Ok");
}

TEST(UtilityFunctionsTest, ErrorStringDimensionMismatch) {
    EXPECT_STREQ(lynx::error_string(lynx::ErrorCode::DimensionMismatch),
                 "Dimension mismatch");
}

TEST(UtilityFunctionsTest, ErrorStringVectorNotFound) {
    EXPECT_STREQ(lynx::error_string(lynx::ErrorCode::VectorNotFound),
                 "Vector not found");
}

TEST(UtilityFunctionsTest, ErrorStringIndexNotBuilt) {
    EXPECT_STREQ(lynx::error_string(lynx::ErrorCode::IndexNotBuilt),
                 "Index not built");
}

TEST(UtilityFunctionsTest, ErrorStringInvalidParameter) {
    EXPECT_STREQ(lynx::error_string(lynx::ErrorCode::InvalidParameter),
                 "Invalid parameter");
}

TEST(UtilityFunctionsTest, ErrorStringInvalidState) {
    EXPECT_STREQ(lynx::error_string(lynx::ErrorCode::InvalidState),
                 "Invalid state");
}

TEST(UtilityFunctionsTest, ErrorStringOutOfMemory) {
    EXPECT_STREQ(lynx::error_string(lynx::ErrorCode::OutOfMemory),
                 "Out of memory");
}

TEST(UtilityFunctionsTest, ErrorStringIOError) {
    EXPECT_STREQ(lynx::error_string(lynx::ErrorCode::IOError),
                 "I/O error");
}

TEST(UtilityFunctionsTest, ErrorStringNotImplemented) {
    EXPECT_STREQ(lynx::error_string(lynx::ErrorCode::NotImplemented),
                 "Not implemented");
}

// ============================================================================
// Index Type String Tests
// ============================================================================

TEST(UtilityFunctionsTest, IndexTypeStringFlat) {
    EXPECT_STREQ(lynx::index_type_string(lynx::IndexType::Flat), "Flat");
}

TEST(UtilityFunctionsTest, IndexTypeStringHNSW) {
    EXPECT_STREQ(lynx::index_type_string(lynx::IndexType::HNSW), "HNSW");
}

TEST(UtilityFunctionsTest, IndexTypeStringIVF) {
    EXPECT_STREQ(lynx::index_type_string(lynx::IndexType::IVF), "IVF");
}

// ============================================================================
// Distance Metric String Tests
// ============================================================================

TEST(UtilityFunctionsTest, DistanceMetricStringL2) {
    EXPECT_STREQ(lynx::distance_metric_string(lynx::DistanceMetric::L2),
                 "L2 (Euclidean)");
}

TEST(UtilityFunctionsTest, DistanceMetricStringCosine) {
    EXPECT_STREQ(lynx::distance_metric_string(lynx::DistanceMetric::Cosine),
                 "Cosine");
}

TEST(UtilityFunctionsTest, DistanceMetricStringDotProduct) {
    EXPECT_STREQ(lynx::distance_metric_string(lynx::DistanceMetric::DotProduct),
                 "Dot Product");
}

// ============================================================================
// Version Tests
// ============================================================================

TEST(UtilityFunctionsTest, VersionNotNull) {
    const char* version = lynx::IVectorDatabase::version();
    EXPECT_NE(version, nullptr);
}

TEST(UtilityFunctionsTest, VersionFormat) {
    const char* version = lynx::IVectorDatabase::version();
    EXPECT_STREQ(version, "0.1.0");
}
