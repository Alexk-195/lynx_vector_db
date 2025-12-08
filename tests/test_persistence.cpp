/**
 * @file test_persistence.cpp
 * @brief Persistence tests for save/load/flush operations
 *
 * Tests serialization and deserialization of the HNSW index and database.
 */

#include <gtest/gtest.h>
#include "../src/include/lynx/lynx.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cmath>
#include <random>

using namespace lynx;

// ============================================================================
// Test Fixture for Persistence Tests
// ============================================================================

class PersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test data
        test_data_path_ = "/tmp/lynx_test_" + std::to_string(std::random_device{}());
        std::filesystem::create_directories(test_data_path_);
    }

    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_data_path_)) {
            std::filesystem::remove_all(test_data_path_);
        }
    }

    std::string test_data_path_;
};

// ============================================================================
// Basic Persistence Tests
// ============================================================================

TEST_F(PersistenceTest, SaveEmptyDatabase) {
    Config config;
    config.dimension = 4;
    config.data_path = test_data_path_;

    auto db = IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr);

    // Save empty database
    ErrorCode result = db->save();
    EXPECT_EQ(result, ErrorCode::Ok);

    // Verify files were created
    EXPECT_TRUE(std::filesystem::exists(test_data_path_ + "/index.hnsw"));
    EXPECT_TRUE(std::filesystem::exists(test_data_path_ + "/metadata.dat"));
}

TEST_F(PersistenceTest, SaveAndLoadEmptyDatabase) {
    Config config;
    config.dimension = 4;
    config.data_path = test_data_path_;

    // Create and save empty database
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->save();
        EXPECT_EQ(result, ErrorCode::Ok);
    }

    // Load into new database
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->load();
        EXPECT_EQ(result, ErrorCode::Ok);
        EXPECT_EQ(db->size(), 0);
    }
}

TEST_F(PersistenceTest, SaveAndLoadSingleVector) {
    Config config;
    config.dimension = 4;
    config.data_path = test_data_path_;

    std::vector<float> vector1 = {1.0f, 2.0f, 3.0f, 4.0f};
    uint64_t id1 = 100;

    // Create, insert, and save
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        VectorRecord record;
        record.id = id1;
        record.vector = vector1;
        record.metadata = "test vector 1";

        ErrorCode result = db->insert(record);
        EXPECT_EQ(result, ErrorCode::Ok);
        EXPECT_EQ(db->size(), 1);

        result = db->save();
        EXPECT_EQ(result, ErrorCode::Ok);
    }

    // Load and verify
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->load();
        EXPECT_EQ(result, ErrorCode::Ok);
        EXPECT_EQ(db->size(), 1);
        EXPECT_TRUE(db->contains(id1));

        auto retrieved = db->get(id1);
        EXPECT_TRUE(retrieved.has_value());
        EXPECT_EQ(retrieved->id, id1);
        EXPECT_EQ(retrieved->metadata.value_or(""), "test vector 1");
    }
}

TEST_F(PersistenceTest, SaveAndLoadMultipleVectors) {
    Config config;
    config.dimension = 8;
    config.data_path = test_data_path_;

    const size_t num_vectors = 100;
    std::vector<VectorRecord> records;

    // Generate test vectors
    for (size_t i = 0; i < num_vectors; ++i) {
        VectorRecord record;
        record.id = i;
        record.vector.resize(config.dimension);
        for (size_t j = 0; j < config.dimension; ++j) {
            record.vector[j] = static_cast<float>(i * config.dimension + j);
        }
        record.metadata = "vector_" + std::to_string(i);
        records.push_back(record);
    }

    // Create, insert, and save
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->batch_insert(records);
        EXPECT_EQ(result, ErrorCode::Ok);
        EXPECT_EQ(db->size(), num_vectors);

        result = db->save();
        EXPECT_EQ(result, ErrorCode::Ok);
    }

    // Load and verify
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->load();
        EXPECT_EQ(result, ErrorCode::Ok);
        EXPECT_EQ(db->size(), num_vectors);

        // Verify all vectors are present
        for (size_t i = 0; i < num_vectors; ++i) {
            EXPECT_TRUE(db->contains(i));
        }
    }
}

TEST_F(PersistenceTest, SaveAndLoadPreservesSearchQuality) {
    Config config;
    config.dimension = 16;
    config.data_path = test_data_path_;
    config.hnsw_params.m = 8;
    config.hnsw_params.ef_construction = 100;

    const size_t num_vectors = 1000;
    std::vector<VectorRecord> records;

    // Generate test vectors
    for (size_t i = 0; i < num_vectors; ++i) {
        VectorRecord record;
        record.id = i;
        record.vector.resize(config.dimension);
        for (size_t j = 0; j < config.dimension; ++j) {
            record.vector[j] = std::sin(static_cast<float>(i + j));
        }
        records.push_back(record);
    }

    SearchResult result_before_save;
    std::vector<float> query_vector(config.dimension);
    for (size_t j = 0; j < config.dimension; ++j) {
        query_vector[j] = std::cos(static_cast<float>(j));
    }

    // Create, insert, search, and save
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->batch_insert(records);
        EXPECT_EQ(result, ErrorCode::Ok);

        result_before_save = db->search(query_vector, 10);
        EXPECT_GT(result_before_save.items.size(), 0);

        result = db->save();
        EXPECT_EQ(result, ErrorCode::Ok);
    }

    // Load and verify search results match
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->load();
        EXPECT_EQ(result, ErrorCode::Ok);

        SearchResult result_after_load = db->search(query_vector, 10);
        EXPECT_EQ(result_after_load.items.size(), result_before_save.items.size());

        // Verify that the top results are the same (order and IDs)
        for (size_t i = 0; i < result_before_save.items.size(); ++i) {
            EXPECT_EQ(result_after_load.items[i].id, result_before_save.items[i].id);
            EXPECT_NEAR(result_after_load.items[i].distance,
                       result_before_save.items[i].distance,
                       0.001f);
        }
    }
}

TEST_F(PersistenceTest, SaveWithoutDataPath) {
    Config config;
    config.dimension = 4;
    // Don't set data_path

    auto db = IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr);

    // Save should fail without data_path
    ErrorCode result = db->save();
    EXPECT_EQ(result, ErrorCode::InvalidParameter);
}

TEST_F(PersistenceTest, LoadWithoutDataPath) {
    Config config;
    config.dimension = 4;
    // Don't set data_path

    auto db = IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr);

    // Load should fail without data_path
    ErrorCode result = db->load();
    EXPECT_EQ(result, ErrorCode::InvalidParameter);
}

TEST_F(PersistenceTest, LoadNonexistentFile) {
    Config config;
    config.dimension = 4;
    config.data_path = test_data_path_ + "/nonexistent";

    auto db = IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr);

    // Load should fail if files don't exist
    ErrorCode result = db->load();
    EXPECT_EQ(result, ErrorCode::IOError);
}

TEST_F(PersistenceTest, FlushWithoutWAL) {
    Config config;
    config.dimension = 4;
    config.enable_wal = false;

    auto db = IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr);

    // Flush should succeed (no-op) without WAL
    ErrorCode result = db->flush();
    EXPECT_EQ(result, ErrorCode::Ok);
}

TEST_F(PersistenceTest, FlushWithWALNotImplemented) {
    Config config;
    config.dimension = 4;
    config.enable_wal = true;
    config.data_path = test_data_path_;

    auto db = IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr);

    // Flush should return NotImplemented with WAL enabled
    ErrorCode result = db->flush();
    EXPECT_EQ(result, ErrorCode::NotImplemented);
}

TEST_F(PersistenceTest, SaveAndLoadWithDifferentDistanceMetrics) {
    Config config;
    config.dimension = 8;
    config.data_path = test_data_path_;
    config.distance_metric = DistanceMetric::Cosine;

    const size_t num_vectors = 50;
    std::vector<VectorRecord> records;

    // Generate test vectors
    for (size_t i = 0; i < num_vectors; ++i) {
        VectorRecord record;
        record.id = i;
        record.vector.resize(config.dimension);
        for (size_t j = 0; j < config.dimension; ++j) {
            record.vector[j] = static_cast<float>(i + j) / 10.0f;
        }
        records.push_back(record);
    }

    // Create, insert, and save
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->batch_insert(records);
        EXPECT_EQ(result, ErrorCode::Ok);

        result = db->save();
        EXPECT_EQ(result, ErrorCode::Ok);
    }

    // Load and verify metric is preserved
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->load();
        EXPECT_EQ(result, ErrorCode::Ok);
        EXPECT_EQ(db->size(), num_vectors);

        // The loaded database should use the same distance metric
        EXPECT_EQ(db->config().distance_metric, DistanceMetric::Cosine);
    }
}

TEST_F(PersistenceTest, MultipleSavesOverwritePrevious) {
    Config config;
    config.dimension = 4;
    config.data_path = test_data_path_;

    std::vector<float> vector1 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> vector2 = {5.0f, 6.0f, 7.0f, 8.0f};

    // First save with one vector
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        VectorRecord record;
        record.id = 1;
        record.vector = vector1;

        ErrorCode result = db->insert(record);
        EXPECT_EQ(result, ErrorCode::Ok);

        result = db->save();
        EXPECT_EQ(result, ErrorCode::Ok);
    }

    // Second save with different vector (should overwrite)
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        VectorRecord record;
        record.id = 2;
        record.vector = vector2;

        ErrorCode result = db->insert(record);
        EXPECT_EQ(result, ErrorCode::Ok);

        result = db->save();
        EXPECT_EQ(result, ErrorCode::Ok);
    }

    // Load should get the second version
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->load();
        EXPECT_EQ(result, ErrorCode::Ok);
        EXPECT_EQ(db->size(), 1);
        EXPECT_TRUE(db->contains(2));
        EXPECT_FALSE(db->contains(1));
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(PersistenceTest, SaveAndLoadLargeVectorDimension) {
    Config config;
    config.dimension = 1024;
    config.data_path = test_data_path_;

    VectorRecord record;
    record.id = 42;
    record.vector.resize(config.dimension);
    for (size_t i = 0; i < config.dimension; ++i) {
        record.vector[i] = static_cast<float>(i) / config.dimension;
    }

    // Save
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->insert(record);
        EXPECT_EQ(result, ErrorCode::Ok);

        result = db->save();
        EXPECT_EQ(result, ErrorCode::Ok);
    }

    // Load and verify
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->load();
        EXPECT_EQ(result, ErrorCode::Ok);
        EXPECT_EQ(db->size(), 1);
        EXPECT_TRUE(db->contains(42));
    }
}

TEST_F(PersistenceTest, SaveAndLoadVectorsWithoutMetadata) {
    Config config;
    config.dimension = 4;
    config.data_path = test_data_path_;

    const size_t num_vectors = 10;
    std::vector<VectorRecord> records;

    // Generate test vectors without metadata
    for (size_t i = 0; i < num_vectors; ++i) {
        VectorRecord record;
        record.id = i;
        record.vector.resize(config.dimension);
        for (size_t j = 0; j < config.dimension; ++j) {
            record.vector[j] = static_cast<float>(i + j);
        }
        // No metadata
        records.push_back(record);
    }

    // Save
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->batch_insert(records);
        EXPECT_EQ(result, ErrorCode::Ok);

        result = db->save();
        EXPECT_EQ(result, ErrorCode::Ok);
    }

    // Load and verify
    {
        auto db = IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr);

        ErrorCode result = db->load();
        EXPECT_EQ(result, ErrorCode::Ok);
        EXPECT_EQ(db->size(), num_vectors);
    }
}
