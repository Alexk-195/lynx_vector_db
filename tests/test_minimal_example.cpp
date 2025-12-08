/**
 * @file test_minimal_example.cpp
 * @brief Test to verify minimal example functionality
 *
 * This test ensures that the minimal example code works correctly
 * by running the same workflow and verifying expected behavior.
 */

#include <gtest/gtest.h>
#include "lynx/lynx.h"
#include <vector>

/**
 * @brief Test the minimal example workflow
 *
 * This test replicates the minimal example's workflow to ensure
 * it produces correct results without running the executable.
 */
TEST(MinimalExampleTest, BasicWorkflow) {
    // 1. Configure database
    lynx::Config config;
    config.dimension = 4;
    config.index_type = lynx::IndexType::HNSW;
    config.distance_metric = lynx::DistanceMetric::L2;

    // 2. Create database
    auto db = lynx::IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr) << "Database creation should succeed";
    EXPECT_EQ(db->dimension(), 4) << "Database dimension should match config";

    // 3. Insert vectors
    lynx::VectorRecord rec1;
    rec1.id = 1;
    rec1.vector = {1.0f, 0.0f, 0.0f, 0.0f};
    auto err1 = db->insert(rec1);
    EXPECT_EQ(err1, lynx::ErrorCode::Ok) << "Insert of vector 1 should succeed";

    lynx::VectorRecord rec2;
    rec2.id = 2;
    rec2.vector = {0.0f, 1.0f, 0.0f, 0.0f};
    auto err2 = db->insert(rec2);
    EXPECT_EQ(err2, lynx::ErrorCode::Ok) << "Insert of vector 2 should succeed";

    lynx::VectorRecord rec3;
    rec3.id = 3;
    rec3.vector = {0.9f, 0.1f, 0.0f, 0.0f};
    auto err3 = db->insert(rec3);
    EXPECT_EQ(err3, lynx::ErrorCode::Ok) << "Insert of vector 3 should succeed";

    // Verify database size
    EXPECT_EQ(db->size(), 3) << "Database should contain 3 vectors";

    // 4. Search for nearest neighbors
    std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
    auto results = db->search(query, 2);

    // Verify search results
    ASSERT_GE(results.items.size(), 1) << "Should find at least 1 result";
    ASSERT_LE(results.items.size(), 2) << "Should find at most 2 results";

    // The first result should be vector 1 (exact match, distance 0)
    EXPECT_EQ(results.items[0].id, 1) << "First result should be vector 1";
    EXPECT_NEAR(results.items[0].distance, 0.0f, 0.001f)
        << "Vector 1 should have distance ~0 (exact match)";

    // If there's a second result, it should be vector 3 (closer than vector 2)
    if (results.items.size() > 1) {
        EXPECT_EQ(results.items[1].id, 3) << "Second result should be vector 3";
        // Vector 3 is [0.9, 0.1, 0, 0], distance to [1, 0, 0, 0] is sqrt(0.01 + 0.01) = ~0.141
        EXPECT_NEAR(results.items[1].distance, 0.1414f, 0.01f)
            << "Vector 3 should have distance ~0.141";
    }
}

/**
 * @brief Test that minimal example handles errors gracefully
 */
TEST(MinimalExampleTest, ErrorHandling) {
    lynx::Config config;
    config.dimension = 4;
    config.index_type = lynx::IndexType::HNSW;
    config.distance_metric = lynx::DistanceMetric::L2;

    auto db = lynx::IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr);

    // Try to insert a vector with wrong dimension
    lynx::VectorRecord bad_record;
    bad_record.id = 999;
    bad_record.vector = {1.0f, 2.0f};  // Wrong dimension (2 instead of 4)

    auto err = db->insert(bad_record);
    EXPECT_NE(err, lynx::ErrorCode::Ok)
        << "Insert with wrong dimension should fail";
    EXPECT_EQ(err, lynx::ErrorCode::DimensionMismatch)
        << "Error should be DimensionMismatch";

    // Try to search with wrong dimension
    std::vector<float> bad_query = {1.0f, 2.0f};  // Wrong dimension
    auto results = db->search(bad_query, 5);
    EXPECT_EQ(results.items.size(), 0)
        << "Search with wrong dimension should return no results";
}

/**
 * @brief Test that contains() works as expected
 */
TEST(MinimalExampleTest, ContainsCheck) {
    lynx::Config config;
    config.dimension = 4;
    auto db = lynx::IVectorDatabase::create(config);
    ASSERT_NE(db, nullptr);

    // Database should be empty
    EXPECT_FALSE(db->contains(1)) << "Vector 1 should not exist yet";

    // Insert a vector
    lynx::VectorRecord rec;
    rec.id = 1;
    rec.vector = {1.0f, 0.0f, 0.0f, 0.0f};
    db->insert(rec);

    // Now it should exist
    EXPECT_TRUE(db->contains(1)) << "Vector 1 should exist after insert";
    EXPECT_FALSE(db->contains(999)) << "Vector 999 should not exist";
}

/**
 * @brief Test that the minimal example can be repeated
 */
TEST(MinimalExampleTest, Repeatability) {
    for (int iteration = 0; iteration < 3; ++iteration) {
        lynx::Config config;
        config.dimension = 4;
        auto db = lynx::IVectorDatabase::create(config);
        ASSERT_NE(db, nullptr) << "Iteration " << iteration
                                << ": Database creation should succeed";

        lynx::VectorRecord rec;
        rec.id = 1;
        rec.vector = {1.0f, 0.0f, 0.0f, 0.0f};
        auto err = db->insert(rec);
        EXPECT_EQ(err, lynx::ErrorCode::Ok) << "Iteration " << iteration
                                            << ": Insert should succeed";

        std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
        auto results = db->search(query, 1);
        ASSERT_GE(results.items.size(), 1) << "Iteration " << iteration
                                           << ": Should find at least 1 result";
        EXPECT_EQ(results.items[0].id, 1) << "Iteration " << iteration
                                          << ": Result should be vector 1";
    }
}
