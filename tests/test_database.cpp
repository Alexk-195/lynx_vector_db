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
        config.ivf_params.n_clusters = 10;
        config.ivf_params.n_probe = 3;
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

TEST(VectorDatabaseTest, InsertDuplicateIdRejected) {
    lynx::Config config;
    config.dimension = 2;
    auto db = lynx::IVectorDatabase::create(config);

    EXPECT_EQ(db->insert({1, {1.0f, 0.0f}, std::nullopt}), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->insert({1, {0.0f, 1.0f}, std::nullopt}), lynx::ErrorCode::InvalidParameter);

    EXPECT_EQ(db->size(), 1); // Still only 1 vector

    auto retrieved = db->get(1);
    ASSERT_TRUE(retrieved.has_value());
    // Original vector should still be there
    EXPECT_FLOAT_EQ(retrieved->vector[0], 1.0f);
    EXPECT_FLOAT_EQ(retrieved->vector[1], 0.0f);
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
    records.push_back({2, {1.0f, 0.0f}, std::nullopt});

    auto result = db->batch_insert(records);
    EXPECT_EQ(result, lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 2);
    

    records.clear();
    records.push_back({3, {1.0f, 0.0f}, std::nullopt});
    records.push_back({4, {1.0f, 0.0f}, std::nullopt});
    records.push_back({5, {0.0f, 1.0f, 2.0f}, std::nullopt}); // Wrong dimension

    result = db->batch_insert(records);
    EXPECT_EQ(result, lynx::ErrorCode::DimensionMismatch);
    // Complete batch should be rejected
    EXPECT_EQ(db->size(), 2);
   
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

TEST(VectorDatabaseTest, SaveAndLoadWithDataPath) {
    // Use a temporary directory under /tmp
    const std::string test_path = "/tmp/lynx_test_save_load_001";

    // Clean up any existing test files
    std::system(("rm -rf " + test_path).c_str());

    // Create database with data path
    lynx::Config config;
    config.dimension = 3;
    config.distance_metric = lynx::DistanceMetric::L2;
    config.data_path = test_path;

    // Create, insert, and save in first scope
    {
        auto db1 = lynx::IVectorDatabase::create(config);
        ASSERT_NE(db1, nullptr);

        // Insert test data
        EXPECT_EQ(db1->insert({1, {1.0f, 0.0f, 0.0f}, std::nullopt}), lynx::ErrorCode::Ok);
        EXPECT_EQ(db1->insert({2, {0.0f, 1.0f, 0.0f}, std::nullopt}), lynx::ErrorCode::Ok);
        EXPECT_EQ(db1->insert({3, {0.0f, 0.0f, 1.0f}, std::nullopt}), lynx::ErrorCode::Ok);
        EXPECT_EQ(db1->size(), 3);

        // Save database
        auto save_result = db1->save();
        EXPECT_EQ(save_result, lynx::ErrorCode::Ok);
    }

    // Load in second scope
    {
        auto db2 = lynx::IVectorDatabase::create(config);
        ASSERT_NE(db2, nullptr);
        EXPECT_EQ(db2->size(), 0); // Should be empty before load

        // Load the saved database
        auto load_result = db2->load();
        EXPECT_EQ(load_result, lynx::ErrorCode::Ok);

        // Verify loaded data
        EXPECT_EQ(db2->size(), 3);
        EXPECT_TRUE(db2->contains(1));
        EXPECT_TRUE(db2->contains(2));
        EXPECT_TRUE(db2->contains(3));

        // Verify search works after load
        std::vector<float> query = {1.0f, 0.0f, 0.0f};
        auto search_result = db2->search(query, 3);
        EXPECT_EQ(search_result.items.size(), 3);
    }

    // Clean up
    std::system(("rm -rf " + test_path).c_str());
}

TEST(VectorDatabaseTest, SaveAndLoadWithMetadata) {
    const std::string test_path = "/tmp/lynx_test_save_load_002";
    std::system(("rm -rf " + test_path).c_str());

    lynx::Config config;
    config.dimension = 2;
    config.data_path = test_path;

    // Save with metadata
    {
        auto db1 = lynx::IVectorDatabase::create(config);

        // Insert vectors with metadata
        EXPECT_EQ(db1->insert({1, {1.0f, 2.0f}, "{\"name\": \"vector1\"}"}), lynx::ErrorCode::Ok);
        EXPECT_EQ(db1->insert({2, {3.0f, 4.0f}, "{\"name\": \"vector2\"}"}), lynx::ErrorCode::Ok);

        // Save
        EXPECT_EQ(db1->save(), lynx::ErrorCode::Ok);
    }

    // Load and verify
    {
        auto db2 = lynx::IVectorDatabase::create(config);
        EXPECT_EQ(db2->load(), lynx::ErrorCode::Ok);

        // Verify vectors were loaded
        EXPECT_EQ(db2->size(), 2);
        EXPECT_TRUE(db2->contains(1));
        EXPECT_TRUE(db2->contains(2));
    }

    std::system(("rm -rf " + test_path).c_str());
}

TEST(VectorDatabaseTest, SaveAndLoadPreservesSearchResults) {
    const std::string test_path = "/tmp/lynx_test_save_load_003";
    std::system(("rm -rf " + test_path).c_str());

    lynx::Config config;
    config.dimension = 2;
    config.distance_metric = lynx::DistanceMetric::L2;
    config.data_path = test_path;

    auto db1 = lynx::IVectorDatabase::create(config);

    // Insert vectors at different distances from origin
    db1->insert({1, {0.0f, 0.0f}, std::nullopt});
    db1->insert({2, {1.0f, 0.0f}, std::nullopt});
    db1->insert({3, {2.0f, 0.0f}, std::nullopt});
    db1->insert({4, {3.0f, 0.0f}, std::nullopt});

    // Perform search on original database
    std::vector<float> query = {0.0f, 0.0f};
    auto result1 = db1->search(query, 3);

    // Save
    EXPECT_EQ(db1->save(), lynx::ErrorCode::Ok);

    // Load into new database
    auto db2 = lynx::IVectorDatabase::create(config);
    EXPECT_EQ(db2->load(), lynx::ErrorCode::Ok);

    // Perform same search on loaded database
    auto result2 = db2->search(query, 3);

    // Results should be identical
    ASSERT_EQ(result2.items.size(), result1.items.size());
    for (size_t i = 0; i < result1.items.size(); ++i) {
        EXPECT_EQ(result2.items[i].id, result1.items[i].id);
        EXPECT_FLOAT_EQ(result2.items[i].distance, result1.items[i].distance);
    }

    std::system(("rm -rf " + test_path).c_str());
}

TEST(VectorDatabaseTest, SaveAndLoadEmptyDatabase) {
    const std::string test_path = "/tmp/lynx_test_save_load_004";
    std::system(("rm -rf " + test_path).c_str());

    lynx::Config config;
    config.dimension = 4;
    config.data_path = test_path;

    auto db1 = lynx::IVectorDatabase::create(config);
    EXPECT_EQ(db1->size(), 0);

    // Save empty database
    EXPECT_EQ(db1->save(), lynx::ErrorCode::Ok);

    // Load into new database
    auto db2 = lynx::IVectorDatabase::create(config);
    EXPECT_EQ(db2->load(), lynx::ErrorCode::Ok);

    // Should still be empty
    EXPECT_EQ(db2->size(), 0);

    std::system(("rm -rf " + test_path).c_str());
}

TEST(VectorDatabaseTest, SaveAndLoadWithDifferentIndexTypes) {
    const std::string test_path = "/tmp/lynx_test_save_load_005";
    std::system(("rm -rf " + test_path).c_str());

    // Test with HNSW index
    {
        lynx::Config config;
        config.dimension = 3;
        config.index_type = lynx::IndexType::HNSW;
        config.data_path = test_path;

        auto db1 = lynx::IVectorDatabase::create(config);
        db1->insert({1, {1.0f, 2.0f, 3.0f}, std::nullopt});
        db1->insert({2, {4.0f, 5.0f, 6.0f}, std::nullopt});

        EXPECT_EQ(db1->save(), lynx::ErrorCode::Ok);

        auto db2 = lynx::IVectorDatabase::create(config);
        EXPECT_EQ(db2->load(), lynx::ErrorCode::Ok);
        EXPECT_EQ(db2->size(), 2);

        std::system(("rm -rf " + test_path).c_str());
    }

    // Test with Flat index
    {
        lynx::Config config;
        config.dimension = 3;
        config.index_type = lynx::IndexType::Flat;
        config.data_path = test_path;

        auto db1 = lynx::IVectorDatabase::create(config);
        db1->insert({1, {1.0f, 2.0f, 3.0f}, std::nullopt});
        db1->insert({2, {4.0f, 5.0f, 6.0f}, std::nullopt});

        EXPECT_EQ(db1->save(), lynx::ErrorCode::Ok);

        auto db2 = lynx::IVectorDatabase::create(config);
        EXPECT_EQ(db2->load(), lynx::ErrorCode::Ok);
        EXPECT_EQ(db2->size(), 2);

        std::system(("rm -rf " + test_path).c_str());
    }
}

TEST(VectorDatabaseTest, SaveAndLoadLargeDatabase) {
    const std::string test_path = "/tmp/lynx_test_save_load_006";
    std::system(("rm -rf " + test_path).c_str());

    lynx::Config config;
    config.dimension = 128;
    config.data_path = test_path;

    // Save large database
    {
        auto db1 = lynx::IVectorDatabase::create(config);

        // Insert 1000 vectors
        for (uint64_t i = 1; i <= 1000; ++i) {
            std::vector<float> vec(128);
            for (size_t j = 0; j < 128; ++j) {
                vec[j] = static_cast<float>(i * j);
            }
            db1->insert({i, vec, std::nullopt});
        }

        EXPECT_EQ(db1->size(), 1000);

        // Save
        EXPECT_EQ(db1->save(), lynx::ErrorCode::Ok);
    }

    // Load and verify
    {
        auto db2 = lynx::IVectorDatabase::create(config);
        EXPECT_EQ(db2->load(), lynx::ErrorCode::Ok);
        EXPECT_EQ(db2->size(), 1000);

        // Verify IDs are present
        EXPECT_TRUE(db2->contains(1));
        EXPECT_TRUE(db2->contains(500));
        EXPECT_TRUE(db2->contains(1000));

        // Verify search works
        std::vector<float> query(128, 1.0f);
        auto search_result = db2->search(query, 10);
        EXPECT_GT(search_result.items.size(), 0);
    }

    std::system(("rm -rf " + test_path).c_str());
}

TEST(VectorDatabaseTest, SaveAndLoadWithDifferentDistanceMetrics) {
    const std::string test_path = "/tmp/lynx_test_save_load_007";

    // Test with Cosine distance
    {
        std::system(("rm -rf " + test_path).c_str());

        lynx::Config config;
        config.dimension = 2;
        config.distance_metric = lynx::DistanceMetric::Cosine;
        config.data_path = test_path;

        auto db1 = lynx::IVectorDatabase::create(config);
        db1->insert({1, {1.0f, 0.0f}, std::nullopt});
        db1->insert({2, {0.0f, 1.0f}, std::nullopt});

        EXPECT_EQ(db1->save(), lynx::ErrorCode::Ok);

        auto db2 = lynx::IVectorDatabase::create(config);
        EXPECT_EQ(db2->load(), lynx::ErrorCode::Ok);
        EXPECT_EQ(db2->size(), 2);

        std::system(("rm -rf " + test_path).c_str());
    }

    // Test with DotProduct distance
    {
        std::system(("rm -rf " + test_path).c_str());

        lynx::Config config;
        config.dimension = 2;
        config.distance_metric = lynx::DistanceMetric::DotProduct;
        config.data_path = test_path;

        auto db1 = lynx::IVectorDatabase::create(config);
        db1->insert({1, {1.0f, 0.0f}, std::nullopt});
        db1->insert({2, {2.0f, 0.0f}, std::nullopt});

        EXPECT_EQ(db1->save(), lynx::ErrorCode::Ok);

        auto db2 = lynx::IVectorDatabase::create(config);
        EXPECT_EQ(db2->load(), lynx::ErrorCode::Ok);
        EXPECT_EQ(db2->size(), 2);

        std::system(("rm -rf " + test_path).c_str());
    }
}

TEST(VectorDatabaseTest, LoadNonexistentPathReturnsError) {
    const std::string test_path = "/tmp/lynx_test_nonexistent_path";
    std::system(("rm -rf " + test_path).c_str());

    lynx::Config config;
    config.dimension = 3;
    config.data_path = test_path;

    auto db = lynx::IVectorDatabase::create(config);

    // Try to load from non-existent path
    auto result = db->load();
    EXPECT_NE(result, lynx::ErrorCode::Ok);
}

// ============================================================================
// IVF Index Integration Tests
// ============================================================================

TEST(IVFDatabaseTest, CreateWithDefaultParams) {
    lynx::Config config;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 5;
    config.ivf_params.n_probe = 2;

    auto db = lynx::IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(db->dimension(), 128); // Default dimension
    EXPECT_EQ(db->size(), 0);
}

TEST(IVFDatabaseTest, BatchInsertAndSearch) {
    lynx::Config config;
    config.dimension = 64;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 5;
    config.ivf_params.n_probe = 3;

    auto db = lynx::IVectorDatabase::create(config);

    // Create batch of vectors
    std::vector<lynx::VectorRecord> records;
    for (std::size_t i = 0; i < 100; ++i) {
        std::vector<float> vec(64);
        for (std::size_t j = 0; j < 64; ++j) {
            vec[j] = static_cast<float>(i + j) / 100.0f;
        }
        records.push_back({i, std::move(vec), std::nullopt});
    }

    // Build index with batch insert
    EXPECT_EQ(db->batch_insert(records), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 100);

    // Search
    std::vector<float> query(64, 0.5f);
    auto result = db->search(query, 10);

    EXPECT_LE(result.items.size(), 10);
    EXPECT_GT(result.query_time_ms, 0.0);
}

TEST(IVFDatabaseTest, BatchInsertThenIncrementalInsert) {
    lynx::Config config;
    config.dimension = 32;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 3;
    config.ivf_params.n_probe = 2;

    auto db = lynx::IVectorDatabase::create(config);

    // Batch insert to build index
    std::vector<lynx::VectorRecord> records;
    for (std::size_t i = 0; i < 50; ++i) {
        std::vector<float> vec(32, static_cast<float>(i));
        records.push_back({i, std::move(vec), std::nullopt});
    }
    EXPECT_EQ(db->batch_insert(records), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 50);

    // Incremental insert
    std::vector<float> new_vec(32, 999.0f);
    lynx::VectorRecord new_record{100, std::move(new_vec), std::nullopt};
    EXPECT_EQ(db->insert(new_record), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 51);
    EXPECT_TRUE(db->contains(100));
}

TEST(IVFDatabaseTest, IncrementalInsertWithoutBuildAutoInitializes) {
    lynx::Config config;
    config.dimension = 8;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 2;

    auto db = lynx::IVectorDatabase::create(config);

    // Insert without building index first - should auto-initialize
    std::vector<float> vec(8, 1.0f);
    lynx::VectorRecord record{1, vec, std::nullopt};

    // Should succeed with auto-initialization
    EXPECT_EQ(db->insert(record), lynx::ErrorCode::Ok);

    // Verify the vector was inserted
    EXPECT_EQ(db->size(), 1);
    EXPECT_TRUE(db->contains(1));

    // Verify we can search after auto-initialization
    std::vector<float> query(8, 1.0f);
    auto results = db->search(query, 1);
    EXPECT_EQ(results.items.size(), 1);
    EXPECT_EQ(results.items[0].id, 1);

    // Verify we can insert more vectors after auto-initialization
    std::vector<float> vec2(8, 2.0f);
    lynx::VectorRecord record2{2, vec2, std::nullopt};
    EXPECT_EQ(db->insert(record2), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 2);
}

TEST(IVFDatabaseTest, SearchWithDifferentNProbe) {
    lynx::Config config;
    config.dimension = 16;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 10;
    config.ivf_params.n_probe = 3;  // Default n_probe

    auto db = lynx::IVectorDatabase::create(config);

    // Build index
    std::vector<lynx::VectorRecord> records;
    for (std::size_t i = 0; i < 200; ++i) {
        std::vector<float> vec(16);
        for (std::size_t j = 0; j < 16; ++j) {
            vec[j] = static_cast<float>(i * j) / 50.0f;
        }
        records.push_back({i, std::move(vec), std::nullopt});
    }
    EXPECT_EQ(db->batch_insert(records), lynx::ErrorCode::Ok);

    std::vector<float> query(16, 5.0f);

    // Search with n_probe=1 (fast, lower recall)
    lynx::SearchParams params1;
    params1.n_probe = 1;
    auto result1 = db->search(query, 10, params1);
    EXPECT_LE(result1.items.size(), 10);

    // Search with n_probe=5 (slower, higher recall)
    lynx::SearchParams params5;
    params5.n_probe = 5;
    auto result5 = db->search(query, 10, params5);
    EXPECT_LE(result5.items.size(), 10);

    // Higher n_probe should give same or better results (lower top distance)
    if (!result1.items.empty() && !result5.items.empty()) {
        EXPECT_LE(result5.items[0].distance, result1.items[0].distance);
    }
}

TEST(IVFDatabaseTest, RemoveAfterBatchInsert) {
    lynx::Config config;
    config.dimension = 8;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 3;
    config.ivf_params.n_probe = 2;

    auto db = lynx::IVectorDatabase::create(config);

    // Build index
    std::vector<lynx::VectorRecord> records;
    for (std::size_t i = 0; i < 30; ++i) {
        std::vector<float> vec(8, static_cast<float>(i));
        records.push_back({i, std::move(vec), std::nullopt});
    }
    EXPECT_EQ(db->batch_insert(records), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 30);

    // Remove a vector
    EXPECT_EQ(db->remove(15), lynx::ErrorCode::Ok);
    EXPECT_EQ(db->size(), 29);
    EXPECT_FALSE(db->contains(15));

    // Search should not return removed vector
    std::vector<float> query(8, 15.0f);
    lynx::SearchParams params;
    params.n_probe = 3;
    auto result = db->search(query, 30, params);

    for (const auto& item : result.items) {
        EXPECT_NE(item.id, 15);
    }
}

TEST(IVFDatabaseTest, StatsAfterOperations) {
    lynx::Config config;
    config.dimension = 16;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 4;

    auto db = lynx::IVectorDatabase::create(config);

    // Initial stats
    auto stats1 = db->stats();
    EXPECT_EQ(stats1.vector_count, 0);
    EXPECT_EQ(stats1.total_queries, 0);

    // Build index
    std::vector<lynx::VectorRecord> records;
    for (std::size_t i = 0; i < 40; ++i) {
        std::vector<float> vec(16, static_cast<float>(i));
        records.push_back({i, std::move(vec), std::nullopt});
    }
    db->batch_insert(records);

    // Stats after insert
    auto stats2 = db->stats();
    EXPECT_EQ(stats2.vector_count, 40);
    EXPECT_GT(stats2.index_memory_bytes, 0);

    // Perform multiple searches to accumulate measurable time
    std::vector<float> query(16, 20.0f);
    lynx::SearchParams params;
    params.n_probe = 2;

    // Run 100 searches to ensure timer captures meaningful time
    for (int i = 0; i < 100; ++i) {
        db->search(query, 5, params);
    }

    // Stats after searches
    auto stats3 = db->stats();
    EXPECT_EQ(stats3.total_queries, 100);
    EXPECT_GT(stats3.avg_query_time_ms, 0.0);
}

TEST(IVFDatabaseTest, DifferentDistanceMetrics) {
    // Test L2
    {
        lynx::Config config;
        config.dimension = 4;
        config.index_type = lynx::IndexType::IVF;
        config.distance_metric = lynx::DistanceMetric::L2;
        config.ivf_params.n_clusters = 2;

        auto db = lynx::IVectorDatabase::create(config);

        std::vector<lynx::VectorRecord> records = {
            {1, {0.0f, 0.0f, 0.0f, 0.0f}, std::nullopt},
            {2, {1.0f, 0.0f, 0.0f, 0.0f}, std::nullopt},
            {3, {2.0f, 0.0f, 0.0f, 0.0f}, std::nullopt}
        };
        db->batch_insert(records);

        std::vector<float> query = {0.0f, 0.0f, 0.0f, 0.0f};
        lynx::SearchParams params;
        params.n_probe = 2;
        auto result = db->search(query, 3, params);

        ASSERT_GE(result.items.size(), 1);
        EXPECT_EQ(result.items[0].id, 1); // Nearest
    }

    // Test Cosine
    {
        lynx::Config config;
        config.dimension = 4;
        config.index_type = lynx::IndexType::IVF;
        config.distance_metric = lynx::DistanceMetric::Cosine;
        config.ivf_params.n_clusters = 2;

        auto db = lynx::IVectorDatabase::create(config);

        std::vector<lynx::VectorRecord> records = {
            {1, {1.0f, 0.0f, 0.0f, 0.0f}, std::nullopt},
            {2, {0.9f, 0.1f, 0.0f, 0.0f}, std::nullopt},
            {3, {0.0f, 1.0f, 0.0f, 0.0f}, std::nullopt}
        };
        db->batch_insert(records);

        std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
        lynx::SearchParams params;
        params.n_probe = 2;
        auto result = db->search(query, 3, params);

        ASSERT_GE(result.items.size(), 1);
        EXPECT_EQ(result.items[0].id, 1); // Same direction
    }
}

TEST(IVFDatabaseTest, PersistenceRoundTrip) {
    const std::string test_path = "/tmp/lynx_test_ivf_persistence";
    std::system(("rm -rf " + test_path).c_str());

    lynx::Config config;
    config.dimension = 16;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 4;
    config.ivf_params.n_probe = 2;
    config.data_path = test_path;

    // Create and populate database
    {
        auto db = lynx::IVectorDatabase::create(config);

        std::vector<lynx::VectorRecord> records;
        for (std::size_t i = 0; i < 50; ++i) {
            std::vector<float> vec(16, static_cast<float>(i) / 10.0f);
            records.push_back({i, std::move(vec), std::nullopt});
        }
        EXPECT_EQ(db->batch_insert(records), lynx::ErrorCode::Ok);
        EXPECT_EQ(db->size(), 50);

        // Save to disk
        EXPECT_EQ(db->save(), lynx::ErrorCode::Ok);
    }

    // Load from disk in new database
    {
        auto db = lynx::IVectorDatabase::create(config);
        EXPECT_EQ(db->load(), lynx::ErrorCode::Ok);
        EXPECT_EQ(db->size(), 50);

        // Verify search works after loading
        std::vector<float> query(16, 2.5f);
        lynx::SearchParams params;
        params.n_probe = 3;
        auto result = db->search(query, 10, params);

        EXPECT_LE(result.items.size(), 10);
    }

    std::system(("rm -rf " + test_path).c_str());
}
