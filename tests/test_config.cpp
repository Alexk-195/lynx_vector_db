/**
 * @file test_config.cpp
 * @brief Unit tests for Lynx configuration structures
 */

#include <gtest/gtest.h>
#include "lynx/lynx.h"

// ============================================================================
// Config Default Values Tests
// ============================================================================

TEST(ConfigTest, DefaultDimension) {
    lynx::Config config;
    EXPECT_EQ(config.dimension, 128);
}

TEST(ConfigTest, DefaultDistanceMetric) {
    lynx::Config config;
    EXPECT_EQ(config.distance_metric, lynx::DistanceMetric::L2);
}

TEST(ConfigTest, DefaultIndexType) {
    lynx::Config config;
    EXPECT_EQ(config.index_type, lynx::IndexType::HNSW);
}

TEST(ConfigTest, DefaultNumQueryThreads) {
    lynx::Config config;
    EXPECT_EQ(config.num_query_threads, 0); // Auto-detect
}

TEST(ConfigTest, DefaultNumIndexThreads) {
    lynx::Config config;
    EXPECT_EQ(config.num_index_threads, 2);
}

TEST(ConfigTest, DefaultDataPath) {
    lynx::Config config;
    EXPECT_TRUE(config.data_path.empty()); // In-memory by default
}

TEST(ConfigTest, DefaultEnableWAL) {
    lynx::Config config;
    EXPECT_FALSE(config.enable_wal);
}

TEST(ConfigTest, DefaultEnableMmap) {
    lynx::Config config;
    EXPECT_FALSE(config.enable_mmap);
}

// ============================================================================
// HNSW Params Default Values Tests
// ============================================================================

TEST(HNSWParamsTest, DefaultM) {
    lynx::HNSWParams params;
    EXPECT_EQ(params.m, 16);
}

TEST(HNSWParamsTest, DefaultEfConstruction) {
    lynx::HNSWParams params;
    EXPECT_EQ(params.ef_construction, 200);
}

TEST(HNSWParamsTest, DefaultEfSearch) {
    lynx::HNSWParams params;
    EXPECT_EQ(params.ef_search, 50);
}

TEST(HNSWParamsTest, DefaultMaxElements) {
    lynx::HNSWParams params;
    EXPECT_EQ(params.max_elements, 1000000);
}

// ============================================================================
// IVF Params Default Values Tests
// ============================================================================

TEST(IVFParamsTest, DefaultNClusters) {
    lynx::IVFParams params;
    EXPECT_EQ(params.n_clusters, 1024);
}

TEST(IVFParamsTest, DefaultNProbe) {
    lynx::IVFParams params;
    EXPECT_EQ(params.n_probe, 10);
}

TEST(IVFParamsTest, DefaultUsePQ) {
    lynx::IVFParams params;
    EXPECT_FALSE(params.use_pq);
}

TEST(IVFParamsTest, DefaultPQSubvectors) {
    lynx::IVFParams params;
    EXPECT_EQ(params.pq_subvectors, 8);
}

// ============================================================================
// Search Params Default Values Tests
// ============================================================================

TEST(SearchParamsTest, DefaultEfSearch) {
    lynx::SearchParams params;
    EXPECT_EQ(params.ef_search, 50);
}

TEST(SearchParamsTest, DefaultNProbe) {
    lynx::SearchParams params;
    EXPECT_EQ(params.n_probe, 10);
}

TEST(SearchParamsTest, DefaultFilter) {
    lynx::SearchParams params;
    EXPECT_FALSE(params.filter.has_value());
}

// ============================================================================
// Config Customization Tests
// ============================================================================

TEST(ConfigTest, CustomDimension) {
    lynx::Config config;
    config.dimension = 512;
    EXPECT_EQ(config.dimension, 512);
}

TEST(ConfigTest, CustomDistanceMetric) {
    lynx::Config config;
    config.distance_metric = lynx::DistanceMetric::Cosine;
    EXPECT_EQ(config.distance_metric, lynx::DistanceMetric::Cosine);
}

TEST(ConfigTest, CustomIndexType) {
    lynx::Config config;
    config.index_type = lynx::IndexType::Flat;
    EXPECT_EQ(config.index_type, lynx::IndexType::Flat);
}

TEST(ConfigTest, CustomDataPath) {
    lynx::Config config;
    config.data_path = "/tmp/lynx_db";
    EXPECT_EQ(config.data_path, "/tmp/lynx_db");
}

TEST(ConfigTest, CustomEnableWAL) {
    lynx::Config config;
    config.enable_wal = true;
    EXPECT_TRUE(config.enable_wal);
}
