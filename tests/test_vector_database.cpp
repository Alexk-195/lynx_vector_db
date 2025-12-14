/**
 * @file test_vector_database.cpp
 * @brief Unit tests for the unified VectorDatabase class
 *
 * Tests the unified VectorDatabase implementation with all three index types
 * (Flat, HNSW, IVF) to verify delegation and common functionality.
 *
 * @copyright MIT License
 */

#include <gtest/gtest.h>
#include "../src/lib/vector_database.h"
#include <vector>
#include <cmath>
#include <filesystem>
#include <memory>

using namespace lynx;

// =============================================================================
// Test Fixtures
// =============================================================================

/**
 * @brief Test fixture for unified VectorDatabase with configurable index type
 */
class UnifiedVectorDatabaseTest : public ::testing::TestWithParam<IndexType> {
protected:
    void SetUp() override {
        config_.dimension = 4;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = GetParam();

        // Set reasonable parameters for each index type
        config_.hnsw_params.m = 8;
        config_.hnsw_params.ef_construction = 100;
        config_.hnsw_params.ef_search = 50;

        config_.ivf_params.n_clusters = 10;
        config_.ivf_params.n_probe = 3;

        db_ = std::make_shared<VectorDatabase>(config_);
    }

    Config config_;
    std::shared_ptr<VectorDatabase> db_;
};

// =============================================================================
// Basic Operations Tests
// =============================================================================

TEST_P(UnifiedVectorDatabaseTest, InsertAndContains) {
    VectorRecord record{1, {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt};

    EXPECT_EQ(db_->insert(record), ErrorCode::Ok);
    EXPECT_TRUE(db_->contains(1));
    EXPECT_FALSE(db_->contains(2));
    EXPECT_EQ(db_->size(), 1);
}

TEST_P(UnifiedVectorDatabaseTest, InsertDuplicateId) {
    VectorRecord record1{1, {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt};
    VectorRecord record2{1, {5.0f, 6.0f, 7.0f, 8.0f}, std::nullopt};

    EXPECT_EQ(db_->insert(record1), ErrorCode::Ok);
    EXPECT_EQ(db_->insert(record2), ErrorCode::InvalidParameter);
    EXPECT_EQ(db_->size(), 1);
}

TEST_P(UnifiedVectorDatabaseTest, InsertWrongDimension) {
    VectorRecord record{1, {1.0f, 2.0f, 3.0f}, std::nullopt};  // Wrong dimension

    EXPECT_EQ(db_->insert(record), ErrorCode::DimensionMismatch);
    EXPECT_EQ(db_->size(), 0);
}

TEST_P(UnifiedVectorDatabaseTest, Get) {
    VectorRecord record{1, {1.0f, 2.0f, 3.0f, 4.0f}, "test metadata"};

    EXPECT_EQ(db_->insert(record), ErrorCode::Ok);

    auto retrieved = db_->get(1);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, 1);
    EXPECT_EQ(retrieved->vector.size(), 4);
    EXPECT_FLOAT_EQ(retrieved->vector[0], 1.0f);
    EXPECT_TRUE(retrieved->metadata.has_value());
    EXPECT_EQ(retrieved->metadata.value(), "test metadata");

    auto missing = db_->get(999);
    EXPECT_FALSE(missing.has_value());
}

TEST_P(UnifiedVectorDatabaseTest, Remove) {
    VectorRecord record{1, {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt};

    EXPECT_EQ(db_->insert(record), ErrorCode::Ok);
    EXPECT_TRUE(db_->contains(1));
    EXPECT_EQ(db_->size(), 1);

    EXPECT_EQ(db_->remove(1), ErrorCode::Ok);
    EXPECT_FALSE(db_->contains(1));
    EXPECT_EQ(db_->size(), 0);
}

TEST_P(UnifiedVectorDatabaseTest, RemoveNonExistent) {
    EXPECT_EQ(db_->remove(999), ErrorCode::VectorNotFound);
}

// =============================================================================
// Search Tests
// =============================================================================

TEST_P(UnifiedVectorDatabaseTest, SearchExactMatch) {
    VectorRecord record{1, {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt};

    EXPECT_EQ(db_->insert(record), ErrorCode::Ok);

    std::vector<float> query = {1.0f, 2.0f, 3.0f, 4.0f};
    auto result = db_->search(query, 1);

    EXPECT_EQ(result.items.size(), 1);
    EXPECT_EQ(result.items[0].id, 1);
    EXPECT_NEAR(result.items[0].distance, 0.0f, 1e-5);  // Exact match
}

TEST_P(UnifiedVectorDatabaseTest, SearchMultipleResults) {
    // Insert 10 vectors
    for (int i = 0; i < 10; ++i) {
        VectorRecord record{static_cast<uint64_t>(i),
                           {i * 1.0f, i * 2.0f, i * 3.0f, i * 4.0f},
                           std::nullopt};
        EXPECT_EQ(db_->insert(record), ErrorCode::Ok);
    }

    // Search for 5 nearest neighbors to {5, 10, 15, 20}
    std::vector<float> query = {5.0f, 10.0f, 15.0f, 20.0f};
    auto result = db_->search(query, 5);

    EXPECT_EQ(result.items.size(), 5);
    // Results should be sorted by distance
    for (size_t i = 1; i < result.items.size(); ++i) {
        EXPECT_LE(result.items[i-1].distance, result.items[i].distance);
    }
}

TEST_P(UnifiedVectorDatabaseTest, SearchWrongDimension) {
    VectorRecord record{1, {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt};
    EXPECT_EQ(db_->insert(record), ErrorCode::Ok);

    std::vector<float> query = {1.0f, 2.0f, 3.0f};  // Wrong dimension
    auto result = db_->search(query, 1);

    EXPECT_EQ(result.items.size(), 0);  // Should return empty result
}

TEST_P(UnifiedVectorDatabaseTest, SearchEmptyDatabase) {
    std::vector<float> query = {1.0f, 2.0f, 3.0f, 4.0f};
    auto result = db_->search(query, 5);

    EXPECT_EQ(result.items.size(), 0);
}

// =============================================================================
// Batch Operations Tests
// =============================================================================

TEST_P(UnifiedVectorDatabaseTest, BatchInsertEmpty) {
    std::vector<VectorRecord> records;

    // Insert into empty database
    EXPECT_EQ(db_->batch_insert(records), ErrorCode::Ok);
    EXPECT_EQ(db_->size(), 0);
}

TEST_P(UnifiedVectorDatabaseTest, BatchInsertIntoEmpty) {
    std::vector<VectorRecord> records;
    for (int i = 0; i < 100; ++i) {
        records.push_back({
            static_cast<uint64_t>(i),
            {i * 1.0f, i * 2.0f, i * 3.0f, i * 4.0f},
            std::nullopt
        });
    }

    // Batch insert into empty database (should use bulk_build)
    EXPECT_EQ(db_->batch_insert(records), ErrorCode::Ok);
    EXPECT_EQ(db_->size(), 100);

    // Verify all vectors are present
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(db_->contains(i));
    }
}

TEST_P(UnifiedVectorDatabaseTest, BatchInsertIncremental) {
    // Insert initial vectors
    for (int i = 0; i < 50; ++i) {
        VectorRecord record{
            static_cast<uint64_t>(i),
            {i * 1.0f, i * 2.0f, i * 3.0f, i * 4.0f},
            std::nullopt
        };
        EXPECT_EQ(db_->insert(record), ErrorCode::Ok);
    }

    // Batch insert small batch (should use incremental)
    std::vector<VectorRecord> records;
    for (int i = 50; i < 60; ++i) {
        records.push_back({
            static_cast<uint64_t>(i),
            {i * 1.0f, i * 2.0f, i * 3.0f, i * 4.0f},
            std::nullopt
        });
    }

    EXPECT_EQ(db_->batch_insert(records), ErrorCode::Ok);
    EXPECT_EQ(db_->size(), 60);
}

TEST_P(UnifiedVectorDatabaseTest, BatchInsertWithDuplicates) {
    VectorRecord record{1, {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt};
    EXPECT_EQ(db_->insert(record), ErrorCode::Ok);

    std::vector<VectorRecord> records = {
        {2, {2.0f, 3.0f, 4.0f, 5.0f}, std::nullopt},
        {1, {3.0f, 4.0f, 5.0f, 6.0f}, std::nullopt},  // Duplicate ID
    };

    EXPECT_EQ(db_->batch_insert(records), ErrorCode::InvalidParameter);
}

// =============================================================================
// Iterator Tests
// =============================================================================

TEST_P(UnifiedVectorDatabaseTest, AllRecords) {
    // Insert some vectors
    for (int i = 0; i < 10; ++i) {
        VectorRecord record{
            static_cast<uint64_t>(i),
            {i * 1.0f, i * 2.0f, i * 3.0f, i * 4.0f},
            std::nullopt
        };
        EXPECT_EQ(db_->insert(record), ErrorCode::Ok);
    }

    // Iterate over all records
    auto records = db_->all_records();
    size_t count = 0;
    for (const auto& [id, record] : records) {
        EXPECT_LT(id, 10);
        EXPECT_EQ(record.vector.size(), 4);
        count++;
    }

    EXPECT_EQ(count, 10);
}

TEST_P(UnifiedVectorDatabaseTest, AllRecordsEmpty) {
    auto records = db_->all_records();
    size_t count = 0;
    for (const auto& [id, record] : records) {
        (void)id;
        (void)record;
        count++;
    }

    EXPECT_EQ(count, 0);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_P(UnifiedVectorDatabaseTest, Statistics) {
    // Insert some vectors
    for (int i = 0; i < 10; ++i) {
        VectorRecord record{
            static_cast<uint64_t>(i),
            {i * 1.0f, i * 2.0f, i * 3.0f, i * 4.0f},
            std::nullopt
        };
        EXPECT_EQ(db_->insert(record), ErrorCode::Ok);
    }

    // Perform some queries
    std::vector<float> query = {5.0f, 10.0f, 15.0f, 20.0f};
    for (int i = 0; i < 5; ++i) {
        db_->search(query, 3);
    }

    auto stats = db_->stats();

    EXPECT_EQ(stats.vector_count, 10);
    EXPECT_EQ(stats.dimension, 4);
    EXPECT_EQ(stats.total_inserts, 10);
    EXPECT_EQ(stats.total_queries, 5);
    EXPECT_GT(stats.avg_query_time_ms, 0.0);
    EXPECT_GT(stats.memory_usage_bytes, 0);
}

TEST_P(UnifiedVectorDatabaseTest, Config) {
    const auto& cfg = db_->config();

    EXPECT_EQ(cfg.dimension, 4);
    EXPECT_EQ(cfg.distance_metric, DistanceMetric::L2);
    EXPECT_EQ(cfg.index_type, GetParam());
}

// =============================================================================
// Persistence Tests
// =============================================================================

class UnifiedVectorDatabasePersistenceTest : public ::testing::TestWithParam<IndexType> {
protected:
    void SetUp() override {
        test_dir_ = "/tmp/lynx_test_" + std::to_string(time(nullptr));
        std::filesystem::create_directories(test_dir_);

        config_.dimension = 4;
        config_.distance_metric = DistanceMetric::L2;
        config_.index_type = GetParam();
        config_.data_path = test_dir_;

        config_.hnsw_params.m = 8;
        config_.hnsw_params.ef_construction = 100;

        config_.ivf_params.n_clusters = 10;
        config_.ivf_params.n_probe = 3;
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir_);
    }

    Config config_;
    std::string test_dir_;
};

TEST_P(UnifiedVectorDatabasePersistenceTest, SaveAndLoad) {
    // Create and populate database
    auto db1 = std::make_shared<VectorDatabase>(config_);

    for (int i = 0; i < 20; ++i) {
        VectorRecord record{
            static_cast<uint64_t>(i),
            {i * 1.0f, i * 2.0f, i * 3.0f, i * 4.0f},
            "metadata_" + std::to_string(i)
        };
        EXPECT_EQ(db1->insert(record), ErrorCode::Ok);
    }

    EXPECT_EQ(db1->save(), ErrorCode::Ok);

    // Load in new database instance
    auto db2 = std::make_shared<VectorDatabase>(config_);
    EXPECT_EQ(db2->load(), ErrorCode::Ok);

    // Verify data
    EXPECT_EQ(db2->size(), 20);
    for (int i = 0; i < 20; ++i) {
        EXPECT_TRUE(db2->contains(i));
        auto record = db2->get(i);
        ASSERT_TRUE(record.has_value());
        EXPECT_EQ(record->vector.size(), 4);
        EXPECT_FLOAT_EQ(record->vector[0], i * 1.0f);
        EXPECT_TRUE(record->metadata.has_value());
        EXPECT_EQ(record->metadata.value(), "metadata_" + std::to_string(i));
    }

    // Verify search still works
    std::vector<float> query = {10.0f, 20.0f, 30.0f, 40.0f};
    auto result = db2->search(query, 5);
    EXPECT_GT(result.items.size(), 0);
}

TEST_P(UnifiedVectorDatabasePersistenceTest, SaveWithoutPath) {
    Config no_path_config = config_;
    no_path_config.data_path = "";

    auto db = std::make_shared<VectorDatabase>(no_path_config);
    EXPECT_EQ(db->save(), ErrorCode::InvalidParameter);
}

TEST_P(UnifiedVectorDatabasePersistenceTest, Flush) {
    auto db = std::make_shared<VectorDatabase>(config_);

    VectorRecord record{1, {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt};
    EXPECT_EQ(db->insert(record), ErrorCode::Ok);

    EXPECT_EQ(db->flush(), ErrorCode::Ok);

    // Verify files were created
    EXPECT_TRUE(std::filesystem::exists(test_dir_ + "/index.bin"));
    EXPECT_TRUE(std::filesystem::exists(test_dir_ + "/vectors.bin"));
}

// =============================================================================
// Test Instantiation
// =============================================================================

INSTANTIATE_TEST_SUITE_P(
    AllIndexTypes,
    UnifiedVectorDatabaseTest,
    ::testing::Values(IndexType::Flat, IndexType::HNSW, IndexType::IVF),
    [](const ::testing::TestParamInfo<IndexType>& info) {
        switch (info.param) {
            case IndexType::Flat: return "Flat";
            case IndexType::HNSW: return "HNSW";
            case IndexType::IVF: return "IVF";
            default: return "Unknown";
        }
    }
);

INSTANTIATE_TEST_SUITE_P(
    AllIndexTypes,
    UnifiedVectorDatabasePersistenceTest,
    ::testing::Values(IndexType::Flat, IndexType::HNSW, IndexType::IVF),
    [](const ::testing::TestParamInfo<IndexType>& info) {
        switch (info.param) {
            case IndexType::Flat: return "Flat";
            case IndexType::HNSW: return "HNSW";
            case IndexType::IVF: return "IVF";
            default: return "Unknown";
        }
    }
);

// =============================================================================
// IVF-Specific Rebuild Tests
// =============================================================================

TEST(UnifiedVectorDatabaseIVFTest, BatchInsertRebuild) {
    Config config;
    config.dimension = 4;
    config.index_type = IndexType::IVF;
    config.ivf_params.n_clusters = 10;

    auto db = std::make_shared<VectorDatabase>(config);

    // Insert initial 50 vectors
    for (int i = 0; i < 50; ++i) {
        VectorRecord record{
            static_cast<uint64_t>(i),
            {i * 1.0f, i * 2.0f, i * 3.0f, i * 4.0f},
            std::nullopt
        };
        EXPECT_EQ(db->insert(record), ErrorCode::Ok);
    }

    // Insert large batch (>50% of existing) - should trigger rebuild
    std::vector<VectorRecord> large_batch;
    for (int i = 50; i < 100; ++i) {
        large_batch.push_back({
            static_cast<uint64_t>(i),
            {i * 1.0f, i * 2.0f, i * 3.0f, i * 4.0f},
            std::nullopt
        });
    }

    EXPECT_EQ(db->batch_insert(large_batch), ErrorCode::Ok);
    EXPECT_EQ(db->size(), 100);

    // Verify search works
    std::vector<float> query = {50.0f, 100.0f, 150.0f, 200.0f};
    auto result = db->search(query, 10);
    EXPECT_GT(result.items.size(), 0);
}
