/**
 * @file test_database.cpp
 * @brief Unit tests for VectorDatabase interface
 */

#include <gtest/gtest.h>
#include "lynx/lynx.h"
#include <memory>
#include <vector>

// ============================================================================
// Database Creation Tests
// ============================================================================

TEST(VectorDatabaseTest, CreateWithDefaultConfig) {
    lynx::Config config;
    auto db = lynx::IVectorDatabase::create(config);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(db->dimension(), 128); // Default dimension
}

TEST(VectorDatabaseTest, CreateWithCustomDimension) {
    lynx::Config config;
    config.dimension = 256;
    auto db = lynx::IVectorDatabase::create(config);

    ASSERT_NE(db, nullptr);
    EXPECT_EQ(db->dimension(), 256);
}

TEST(VectorDatabaseTest, CreateWithDifferentIndexTypes) {
    // Test HNSW
    {
        lynx::Config config;
        config.index_type = lynx::IndexType::HNSW;
        auto db = lynx::IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);
    }

    // Test Flat
    {
        lynx::Config config;
        config.index_type = lynx::IndexType::Flat;
        auto db = lynx::IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);
    }

    // Test IVF
    {
        lynx::Config config;
        config.index_type = lynx::IndexType::IVF;
        auto db = lynx::IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);
    }
}

// ============================================================================
// Database Properties Tests
// ============================================================================

TEST(VectorDatabaseTest, InitialSize) {
    lynx::Config config;
    auto db = lynx::IVectorDatabase::create(config);

    EXPECT_EQ(db->size(), 0); // Empty database
}

TEST(VectorDatabaseTest, ConfigRetrieval) {
    lynx::Config config;
    config.dimension = 384;
    config.distance_metric = lynx::DistanceMetric::Cosine;
    auto db = lynx::IVectorDatabase::create(config);

    const auto& retrieved_config = db->config();
    EXPECT_EQ(retrieved_config.dimension, 384);
    EXPECT_EQ(retrieved_config.distance_metric, lynx::DistanceMetric::Cosine);
}

TEST(VectorDatabaseTest, StatsRetrieval) {
    lynx::Config config;
    auto db = lynx::IVectorDatabase::create(config);

    auto stats = db->stats();
    EXPECT_EQ(stats.vector_count, 0);
    EXPECT_EQ(stats.dimension, config.dimension);
}

// ============================================================================
// Insert Operation Tests
// ============================================================================

TEST(VectorDatabaseTest, InsertSingleVector) {
    lynx::Config config;
    config.dimension = 3;
    auto db = lynx::IVectorDatabase::create(config);

    lynx::VectorRecord record{1, {1.0f, 2.0f, 3.0f}, std::nullopt};
    auto result = db->insert(record);

    EXPECT_EQ(result, lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 1);
    EXPECT_TRUE(db->contains(1));
}

TEST(VectorDatabaseTest, InsertMultipleVectors) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    EXPECT_EQ(db->insert({1, {1.0f, 0.0f}, std::nullopt}), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->insert({2, {0.0f, 1.0f}, std::nullopt}), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->insert({3, {1.0f, 1.0f}, std::nullopt}), lynx::ErrorCode::Ok);

    EXPECT_EQ(db->size(), 3);
}

TEST(VectorDatabaseTest, InsertWithMetadata) {
    lynx::Config config;
    config.dimension = 4;
    auto db = lynx::IVectorDatabase::create(config);

    lynx::VectorRecord record{42, {1.0f, 2.0f, 3.0f, 4.0f}, "{\"name\": \"test\"}"};
    auto result = db->insert(record);

    EXPECT_EQ(result, lynx::ErrorCode::Ok);

    auto retrieved = db->get(42);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_TRUE(retrieved->metadata.has_value());
    EXPECT_EQ(retrieved->metadata.value(), "{\"name\": \"test\"}");
}

TEST(VectorDatabaseTest, InsertDuplicateIdOverwrites) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    EXPECT_EQ(db->insert({1, {1.0f, 0.0f}, std::nullopt}), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->insert({1, {0.0f, 1.0f}, std::nullopt}), lynx::ErrorCode::Ok);

    EXPECT_EQ(db->size(), 1); // Still only 1 vector

    auto retrieved = db->get(1);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FLOAT_EQ(retrieved->vector[0], 0.0f);
    EXPECT_FLOAT_EQ(retrieved->vector[1], 1.0f);
}

TEST(VectorDatabaseTest, InsertWrongDimensionReturnsError) {
    lynx::Config config;
    config.dimension = 3;
    auto db = lynx::IVectorDatabase::create(config);

    // Try to insert 2D vector into 3D database
    lynx::VectorRecord record{1, {1.0f, 2.0f}, std::nullopt};
    auto result = db->insert(record);

    EXPECT_EQ(result, lynx::ErrorCode::DimensionMismatch);
    EXPECT_EQ(db->size(), 0);
}

// ============================================================================
// Contains Operation Tests
// ============================================================================

TEST(VectorDatabaseTest, ContainsReturnsFalseForEmpty) {
    lynx::Config config;
    auto db = lynx::IVectorDatabase::create(config);

    EXPECT_FALSE(db->contains(1));
    EXPECT_FALSE(db->contains(999));
}

TEST(VectorDatabaseTest, ContainsReturnsTrueAfterInsert) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({42, {1.0f, 2.0f}, std::nullopt});

    EXPECT_TRUE(db->contains(42));
    EXPECT_FALSE(db->contains(43));
}

// ============================================================================
// Get Operation Tests
// ============================================================================

TEST(VectorDatabaseTest, GetReturnsNulloptForNonexistent) {
    lynx::Config config;
    auto db = lynx::IVectorDatabase::create(config);

    auto result = db->get(1);
    EXPECT_FALSE(result.has_value());
}

TEST(VectorDatabaseTest, GetReturnsVectorAfterInsert) {
    lynx::Config config;
    config.dimension = 3;
    auto db = lynx::IVectorDatabase::create(config);

    lynx::VectorRecord original{100, {1.5f, 2.5f, 3.5f}, std::nullopt};
    db->insert(original);

    auto retrieved = db->get(100);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, 100);
    EXPECT_EQ(retrieved->vector.size(), 3);
    EXPECT_FLOAT_EQ(retrieved->vector[0], 1.5f);
    EXPECT_FLOAT_EQ(retrieved->vector[1], 2.5f);
    EXPECT_FLOAT_EQ(retrieved->vector[2], 3.5f);
}

TEST(VectorDatabaseTest, GetWithMetadata) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 2.0f}, "{\"key\": \"value\"}"});

    auto result = db->get(1);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->metadata.has_value());
    EXPECT_EQ(result->metadata.value(), "{\"key\": \"value\"}");
}

// ============================================================================
// Remove Operation Tests
// ============================================================================

TEST(VectorDatabaseTest, RemoveExistingVector) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f}, std::nullopt});
    EXPECT_EQ(db->size(), 1);
    EXPECT_TRUE(db->contains(1));

    auto result = db->remove(1);
    EXPECT_EQ(result, lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 0);
    EXPECT_FALSE(db->contains(1));
}

TEST(VectorDatabaseTest, RemoveNonexistentVector) {
    lynx::Config config;
    auto db = lynx::IVectorDatabase::create(config);

    auto result = db->remove(999);
    EXPECT_EQ(result, lynx::ErrorCode::VectorNotFound);
}

TEST(VectorDatabaseTest, RemoveFromMultipleVectors) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f}, std::nullopt});
    db->insert({2, {0.0f, 1.0f}, std::nullopt});
    db->insert({3, {1.0f, 1.0f}, std::nullopt});

    EXPECT_EQ(db->size(), 3);

    db->remove(2);

    EXPECT_EQ(db->size(), 2);
    EXPECT_TRUE(db->contains(1));
    EXPECT_FALSE(db->contains(2));
    EXPECT_TRUE(db->contains(3));
}

// ============================================================================
// Batch Operations Tests
// ============================================================================

TEST(VectorDatabaseTest, BatchInsertMultipleVectors) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    std::vector<lynx::VectorRecord> records;
    records.push_back({1, {1.0f, 0.0f}, std::nullopt});
    records.push_back({2, {0.0f, 1.0f}, std::nullopt});
    records.push_back({3, {1.0f, 1.0f}, std::nullopt});

    auto result = db->batch_insert(records);
    EXPECT_EQ(result, lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 3);
}

TEST(VectorDatabaseTest, BatchInsertWithWrongDimension) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    std::vector<lynx::VectorRecord> records;
    records.push_back({1, {1.0f, 0.0f}, std::nullopt});
    records.push_back({2, {0.0f, 1.0f, 2.0f}, std::nullopt}); // Wrong dimension

    auto result = db->batch_insert(records);
    EXPECT_EQ(result, lynx::ErrorCode::DimensionMismatch);
    // First record should have been inserted before error
    EXPECT_EQ(db->size(), 1);
}

// ============================================================================
// Search Operation Tests
// ============================================================================

TEST(VectorDatabaseTest, SearchEmptyDatabase) {
    lynx::Config config;
    config.dimension = 3;
    auto db = lynx::IVectorDatabase::create(config);

    std::vector<float> query = {1.0f, 0.0f, 0.0f};
    auto result = db->search(query, 5);

    EXPECT_TRUE(result.items.empty());
    EXPECT_EQ(result.total_candidates, 0);
}

TEST(VectorDatabaseTest, SearchSingleVector) {
    lynx::Config config;
    config.dimension = 3;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f, 0.0f}, std::nullopt});

    std::vector<float> query = {1.0f, 0.0f, 0.0f};
    auto result = db->search(query, 5);

    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].id, 1);
    EXPECT_FLOAT_EQ(result.items[0].distance, 0.0f); // Exact match
    EXPECT_EQ(result.total_candidates, 1);
}

TEST(VectorDatabaseTest, SearchReturnsKNearestNeighbors) {
    lynx::Config config;
    config.dimension = 2;
    config.distance_metric = lynx::DistanceMetric::L2;
    auto db = lynx::IVectorDatabase::create(config);

    // Insert 5 vectors with distinct distances from origin
    db->insert({1, {0.0f, 0.0f}, std::nullopt});   // Distance 0.0
    db->insert({2, {1.0f, 0.0f}, std::nullopt});   // Distance 1.0
    db->insert({3, {1.0f, 1.0f}, std::nullopt});   // Distance ~1.414
    db->insert({4, {2.0f, 0.0f}, std::nullopt});   // Distance 2.0
    db->insert({5, {3.0f, 0.0f}, std::nullopt});   // Distance 3.0

    // Query for k=3 nearest to origin
    std::vector<float> query = {0.0f, 0.0f};
    auto result = db->search(query, 3);

    ASSERT_EQ(result.items.size(), 3);
    EXPECT_EQ(result.total_candidates, 5);

    // Should return vectors 1, 2, 3 in order of distance
    EXPECT_EQ(result.items[0].id, 1); // Distance 0
    EXPECT_LT(result.items[0].distance, result.items[1].distance);
    EXPECT_LT(result.items[1].distance, result.items[2].distance);
}

TEST(VectorDatabaseTest, SearchResultsSortedByDistance) {
    lynx::Config config;
    config.dimension = 1;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {5.0f}, std::nullopt});
    db->insert({2, {1.0f}, std::nullopt});
    db->insert({3, {3.0f}, std::nullopt});

    std::vector<float> query = {0.0f};
    auto result = db->search(query, 3);

    ASSERT_EQ(result.items.size(), 3);
    EXPECT_EQ(result.items[0].id, 2); // Closest to 0
    EXPECT_EQ(result.items[1].id, 3);
    EXPECT_EQ(result.items[2].id, 1);
}

TEST(VectorDatabaseTest, SearchWithCosineDistance) {
    lynx::Config config;
    config.dimension = 2;
    config.distance_metric = lynx::DistanceMetric::Cosine;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f}, std::nullopt});
    db->insert({2, {0.0f, 1.0f}, std::nullopt});
    db->insert({3, {1.0f, 1.0f}, std::nullopt});

    std::vector<float> query = {1.0f, 0.0f};
    auto result = db->search(query, 3);

    ASSERT_EQ(result.items.size(), 3);
    EXPECT_EQ(result.items[0].id, 1); // Same direction
}

TEST(VectorDatabaseTest, SearchWithDotProductDistance) {
    lynx::Config config;
    config.dimension = 2;
    config.distance_metric = lynx::DistanceMetric::DotProduct;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f}, std::nullopt});
    db->insert({2, {2.0f, 0.0f}, std::nullopt});

    std::vector<float> query = {1.0f, 0.0f};
    auto result = db->search(query, 2);

    ASSERT_EQ(result.items.size(), 2);
    EXPECT_EQ(result.items[0].id, 2); // Higher dot product (but negative, so lower distance)
}

TEST(VectorDatabaseTest, SearchWithParams) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f}, std::nullopt});
    db->insert({2, {0.0f, 1.0f}, std::nullopt});

    std::vector<float> query = {1.0f, 1.0f};
    lynx::SearchParams params;
    params.ef_search = 100;

    auto result = db->search(query, 2, params);

    ASSERT_EQ(result.items.size(), 2);
}

TEST(VectorDatabaseTest, SearchWithFilter) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f}, std::nullopt});
    db->insert({2, {0.0f, 1.0f}, std::nullopt});
    db->insert({3, {1.0f, 1.0f}, std::nullopt});

    std::vector<float> query = {0.0f, 0.0f};
    lynx::SearchParams params;
    // Only return even IDs
    params.filter = [](uint64_t id) { return id % 2 == 0; };

    auto result = db->search(query, 10, params);

    ASSERT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].id, 2);
}

TEST(VectorDatabaseTest, SearchWrongDimensionReturnsEmpty) {
    lynx::Config config;
    config.dimension = 3;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f, 0.0f}, std::nullopt});

    // Query with wrong dimension
    std::vector<float> query = {1.0f, 0.0f};
    auto result = db->search(query, 5);

    EXPECT_TRUE(result.items.empty());
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST(VectorDatabaseTest, StatsTrackInserts) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f}, std::nullopt});
    db->insert({2, {0.0f, 1.0f}, std::nullopt});

    auto stats = db->stats();
    EXPECT_EQ(stats.vector_count, 2);
    EXPECT_EQ(stats.total_inserts, 2);
}

TEST(VectorDatabaseTest, StatsTrackQueries) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    db->insert({1, {1.0f, 0.0f}, std::nullopt});

    std::vector<float> query = {0.0f, 0.0f};
    db->search(query, 1);
    db->search(query, 1);

    auto stats = db->stats();
    EXPECT_EQ(stats.total_queries, 2);
}

TEST(VectorDatabaseTest, StatsTrackMemoryUsage) {
    lynx::Config config;
    config.dimension = 100;
    auto db = lynx::IVectorDatabase::create(config);

    auto stats1 = db->stats();
    EXPECT_EQ(stats1.memory_usage_bytes, 0);

    db->insert({1, std::vector<float>(100, 1.0f), std::nullopt});

    auto stats2 = db->stats();
    EXPECT_GT(stats2.memory_usage_bytes, 0);
}

// ============================================================================
// Persistence Operations Tests
// ============================================================================

TEST(VectorDatabaseTest, FlushWithoutWALReturnsOk) {
    lynx::Config config;
    auto db = lynx::IVectorDatabase::create(config);

    // Flush without WAL should succeed (no-op)
    auto result = db->flush();
    EXPECT_EQ(result, lynx::ErrorCode::Ok);
}

TEST(VectorDatabaseTest, SaveWithoutDataPathReturnsError) {
    lynx::Config config;
    // Don't set data_path
    auto db = lynx::IVectorDatabase::create(config);

    // Save without data_path should fail
    auto result = db->save();
    EXPECT_EQ(result, lynx::ErrorCode::InvalidParameter);
}

TEST(VectorDatabaseTest, LoadWithoutDataPathReturnsError) {
    lynx::Config config;
    // Don't set data_path
    auto db = lynx::IVectorDatabase::create(config);

    // Load without data_path should fail
    auto result = db->load();
    EXPECT_EQ(result, lynx::ErrorCode::InvalidParameter);
}
