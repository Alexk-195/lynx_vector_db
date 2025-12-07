/**
 * @file test_data_structures.cpp
 * @brief Unit tests for Lynx data structures
 */

#include <gtest/gtest.h>
#include "lynx/lynx.h"
#include <vector>

// ============================================================================
// SearchResultItem Tests
// ============================================================================

TEST(SearchResultItemTest, Construction) {
    lynx::SearchResultItem item{42, 1.5f};
    EXPECT_EQ(item.id, 42);
    EXPECT_FLOAT_EQ(item.distance, 1.5f);
}

TEST(SearchResultItemTest, ZeroDistance) {
    lynx::SearchResultItem item{100, 0.0f};
    EXPECT_EQ(item.id, 100);
    EXPECT_FLOAT_EQ(item.distance, 0.0f);
}

// ============================================================================
// SearchResult Tests
// ============================================================================

TEST(SearchResultTest, EmptyResult) {
    lynx::SearchResult result{{}, 0, 0.0};
    EXPECT_TRUE(result.items.empty());
    EXPECT_EQ(result.total_candidates, 0);
    EXPECT_DOUBLE_EQ(result.query_time_ms, 0.0);
}

TEST(SearchResultTest, WithItems) {
    lynx::SearchResult result;
    result.items.push_back({1, 0.5f});
    result.items.push_back({2, 1.0f});
    result.total_candidates = 100;
    result.query_time_ms = 2.5;

    EXPECT_EQ(result.items.size(), 2);
    EXPECT_EQ(result.items[0].id, 1);
    EXPECT_FLOAT_EQ(result.items[0].distance, 0.5f);
    EXPECT_EQ(result.items[1].id, 2);
    EXPECT_FLOAT_EQ(result.items[1].distance, 1.0f);
    EXPECT_EQ(result.total_candidates, 100);
    EXPECT_DOUBLE_EQ(result.query_time_ms, 2.5);
}

// ============================================================================
// VectorRecord Tests
// ============================================================================

TEST(VectorRecordTest, WithoutMetadata) {
    std::vector<float> vec = {1.0f, 2.0f, 3.0f};
    lynx::VectorRecord record{123, vec, std::nullopt};

    EXPECT_EQ(record.id, 123);
    EXPECT_EQ(record.vector.size(), 3);
    EXPECT_FLOAT_EQ(record.vector[0], 1.0f);
    EXPECT_FLOAT_EQ(record.vector[1], 2.0f);
    EXPECT_FLOAT_EQ(record.vector[2], 3.0f);
    EXPECT_FALSE(record.metadata.has_value());
}

TEST(VectorRecordTest, WithMetadata) {
    std::vector<float> vec = {1.0f, 2.0f};
    lynx::VectorRecord record{456, vec, "{\"label\": \"test\"}"};

    EXPECT_EQ(record.id, 456);
    EXPECT_EQ(record.vector.size(), 2);
    EXPECT_TRUE(record.metadata.has_value());
    EXPECT_EQ(record.metadata.value(), "{\"label\": \"test\"}");
}

TEST(VectorRecordTest, EmptyVector) {
    std::vector<float> vec;
    lynx::VectorRecord record{789, vec, std::nullopt};

    EXPECT_EQ(record.id, 789);
    EXPECT_TRUE(record.vector.empty());
}

// ============================================================================
// DatabaseStats Tests
// ============================================================================

TEST(DatabaseStatsTest, DefaultInitialization) {
    lynx::DatabaseStats stats{};
    EXPECT_EQ(stats.vector_count, 0);
    EXPECT_EQ(stats.dimension, 0);
    EXPECT_EQ(stats.memory_usage_bytes, 0);
    EXPECT_EQ(stats.index_memory_bytes, 0);
    EXPECT_DOUBLE_EQ(stats.avg_query_time_ms, 0.0);
    EXPECT_EQ(stats.total_queries, 0);
    EXPECT_EQ(stats.total_inserts, 0);
}

TEST(DatabaseStatsTest, CustomValues) {
    lynx::DatabaseStats stats;
    stats.vector_count = 1000;
    stats.dimension = 128;
    stats.memory_usage_bytes = 512000;
    stats.index_memory_bytes = 128000;
    stats.avg_query_time_ms = 1.5;
    stats.total_queries = 5000;
    stats.total_inserts = 1000;

    EXPECT_EQ(stats.vector_count, 1000);
    EXPECT_EQ(stats.dimension, 128);
    EXPECT_EQ(stats.memory_usage_bytes, 512000);
    EXPECT_EQ(stats.index_memory_bytes, 128000);
    EXPECT_DOUBLE_EQ(stats.avg_query_time_ms, 1.5);
    EXPECT_EQ(stats.total_queries, 5000);
    EXPECT_EQ(stats.total_inserts, 1000);
}
