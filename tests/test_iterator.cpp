/**
 * @file test_iterator.cpp
 * @brief Unit tests for record iterator functionality
 */

#include <gtest/gtest.h>
#include "lynx/lynx.h"
#include <algorithm>
#include <vector>

// ============================================================================
// Iterator Tests - Flat Database
// ============================================================================

TEST(RecordIteratorTest, FlatDatabase_EmptyDatabase) {
    lynx::Config config;
    config.dimension = 3;
    config.index_type = lynx::IndexType::Flat;
    auto db = lynx::IVectorDatabase::create(config);

    auto records = db->all_records();
    auto it = records.begin();
    auto end = records.end();

    EXPECT_EQ(it, end);
}

TEST(RecordIteratorTest, FlatDatabase_SingleRecord) {
    lynx::Config config;
    config.dimension = 3;
    config.index_type = lynx::IndexType::Flat;
    auto db = lynx::IVectorDatabase::create(config);

    lynx::VectorRecord record{42, {1.0f, 2.0f, 3.0f}, "metadata"};
    ASSERT_EQ(db->insert(record), lynx::ErrorCode::Ok);

    auto records = db->all_records();
    size_t count = 0;
    for (const auto& [id, rec] : records) {
        EXPECT_EQ(id, 42);
        EXPECT_EQ(rec.id, 42);
        EXPECT_EQ(rec.vector.size(), 3);
        EXPECT_EQ(rec.vector[0], 1.0f);
        EXPECT_EQ(rec.vector[1], 2.0f);
        EXPECT_EQ(rec.vector[2], 3.0f);
        EXPECT_TRUE(rec.metadata.has_value());
        EXPECT_EQ(rec.metadata.value(), "metadata");
        count++;
    }
    EXPECT_EQ(count, 1);
}

TEST(RecordIteratorTest, FlatDatabase_MultipleRecords) {
    lynx::Config config;
    config.dimension = 3;
    config.index_type = lynx::IndexType::Flat;
    auto db = lynx::IVectorDatabase::create(config);

    // Insert multiple records
    std::vector<uint64_t> expected_ids = {1, 2, 3, 4, 5};
    for (auto id : expected_ids) {
        lynx::VectorRecord record{id, {static_cast<float>(id), 0.0f, 0.0f}, std::nullopt};
        ASSERT_EQ(db->insert(record), lynx::ErrorCode::Ok);
    }

    // Iterate and collect IDs
    std::vector<uint64_t> found_ids;
    auto records = db->all_records();
    for (const auto& [id, rec] : records) {
        found_ids.push_back(id);
        EXPECT_EQ(id, rec.id);
        EXPECT_EQ(rec.vector.size(), 3);
    }

    // Verify all IDs found (order may vary for unordered_map)
    EXPECT_EQ(found_ids.size(), expected_ids.size());
    std::sort(found_ids.begin(), found_ids.end());
    EXPECT_EQ(found_ids, expected_ids);
}

TEST(RecordIteratorTest, FlatDatabase_RangeBasedForLoop) {
    lynx::Config config;
    config.dimension = 2;
    config.index_type = lynx::IndexType::Flat;
    auto db = lynx::IVectorDatabase::create(config);

    // Insert test data
    for (int i = 0; i < 10; ++i) {
        lynx::VectorRecord record{static_cast<uint64_t>(i), {static_cast<float>(i), static_cast<float>(i * 2)}, std::nullopt};
        ASSERT_EQ(db->insert(record), lynx::ErrorCode::Ok);
    }

    // Verify using range-based for
    size_t count = 0;
    for (const auto& [id, record] : db->all_records()) {
        EXPECT_EQ(id, record.id);
        EXPECT_EQ(record.vector[0], static_cast<float>(id));
        EXPECT_EQ(record.vector[1], static_cast<float>(id * 2));
        count++;
    }
    EXPECT_EQ(count, 10);
}

TEST(RecordIteratorTest, FlatDatabase_AfterRemove) {
    lynx::Config config;
    config.dimension = 3;
    config.index_type = lynx::IndexType::Flat;
    auto db = lynx::IVectorDatabase::create(config);

    // Insert records
    for (int i = 0; i < 5; ++i) {
        lynx::VectorRecord record{static_cast<uint64_t>(i), {1.0f, 2.0f, 3.0f}, std::nullopt};
        ASSERT_EQ(db->insert(record), lynx::ErrorCode::Ok);
    }

    // Remove one record
    ASSERT_EQ(db->remove(2), lynx::ErrorCode::Ok);

    // Verify iterator doesn't include removed record
    std::vector<uint64_t> found_ids;
    for (const auto& [id, rec] : db->all_records()) {
        found_ids.push_back(id);
    }

    EXPECT_EQ(found_ids.size(), 4);
    EXPECT_EQ(std::count(found_ids.begin(), found_ids.end(), 2), 0);
}

TEST(RecordIteratorTest, FlatDatabase_ManualIteration) {
    lynx::Config config;
    config.dimension = 3;
    config.index_type = lynx::IndexType::Flat;
    auto db = lynx::IVectorDatabase::create(config);

    // Insert test data
    for (int i = 0; i < 5; ++i) {
        lynx::VectorRecord record{static_cast<uint64_t>(i), {1.0f, 2.0f, 3.0f}, std::nullopt};
        ASSERT_EQ(db->insert(record), lynx::ErrorCode::Ok);
    }

    // Manual iteration with iterator operations
    auto records = db->all_records();
    auto it = records.begin();
    auto end = records.end();

    size_t count = 0;
    while (it != end) {
        EXPECT_EQ((*it).first, (*it).second.id);
        ++it;
        ++count;
    }
    EXPECT_EQ(count, 5);
}

TEST(RecordIteratorTest, FlatDatabase_IteratorCopy) {
    lynx::Config config;
    config.dimension = 3;
    config.index_type = lynx::IndexType::Flat;
    auto db = lynx::IVectorDatabase::create(config);

    lynx::VectorRecord record{1, {1.0f, 2.0f, 3.0f}, std::nullopt};
    ASSERT_EQ(db->insert(record), lynx::ErrorCode::Ok);

    auto records = db->all_records();
    auto it1 = records.begin();
    auto it2 = it1;  // Copy

    EXPECT_EQ(it1, it2);
    EXPECT_EQ((*it1).first, (*it2).first);
}

// ============================================================================
// Iterator Tests - IVF Database
// ============================================================================

TEST(RecordIteratorTest, IVFDatabase_BasicIteration) {
    lynx::Config config;
    config.dimension = 4;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 5;
    config.ivf_params.n_probe = 2;
    auto db = lynx::IVectorDatabase::create(config);

    // Batch insert to build index
    std::vector<lynx::VectorRecord> records;
    for (int i = 0; i < 20; ++i) {
        records.push_back({static_cast<uint64_t>(i),
                          {static_cast<float>(i), 0.0f, 0.0f, 0.0f},
                          std::nullopt});
    }
    ASSERT_EQ(db->batch_insert(records), lynx::ErrorCode::Ok);

    // Iterate and verify
    std::vector<uint64_t> found_ids;
    for (const auto& [id, rec] : db->all_records()) {
        found_ids.push_back(id);
        EXPECT_EQ(id, rec.id);
    }

    EXPECT_EQ(found_ids.size(), 20);
}

TEST(RecordIteratorTest, IVFDatabase_EmptyDatabase) {
    lynx::Config config;
    config.dimension = 4;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 5;
    auto db = lynx::IVectorDatabase::create(config);

    auto records = db->all_records();
    EXPECT_EQ(records.begin(), records.end());
}

TEST(RecordIteratorTest, IVFDatabase_AfterRemove) {
    lynx::Config config;
    config.dimension = 4;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 5;
    auto db = lynx::IVectorDatabase::create(config);

    // Batch insert
    std::vector<lynx::VectorRecord> records;
    for (int i = 0; i < 10; ++i) {
        records.push_back({static_cast<uint64_t>(i), {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt});
    }
    ASSERT_EQ(db->batch_insert(records), lynx::ErrorCode::Ok);

    // Remove a record
    ASSERT_EQ(db->remove(5), lynx::ErrorCode::Ok);

    // Verify iterator reflects removal
    std::vector<uint64_t> found_ids;
    for (const auto& [id, rec] : db->all_records()) {
        found_ids.push_back(id);
    }

    EXPECT_EQ(found_ids.size(), 9);
    EXPECT_EQ(std::count(found_ids.begin(), found_ids.end(), 5), 0);
}

// ============================================================================
// Iterator Tests - HNSW Database
// ============================================================================

TEST(RecordIteratorTest, HNSWDatabase_BasicIteration) {
    lynx::Config config;
    config.dimension = 4;
    config.index_type = lynx::IndexType::HNSW;
    config.hnsw_params.m = 8;
    config.hnsw_params.ef_construction = 50;
    auto db = lynx::IVectorDatabase::create(config);

    // Insert multiple records
    for (int i = 0; i < 15; ++i) {
        lynx::VectorRecord record{static_cast<uint64_t>(i),
                                 {static_cast<float>(i), 1.0f, 2.0f, 3.0f},
                                 std::nullopt};
        ASSERT_EQ(db->insert(record), lynx::ErrorCode::Ok);
    }

    // Iterate and verify
    std::vector<uint64_t> found_ids;
    for (const auto& [id, rec] : db->all_records()) {
        found_ids.push_back(id);
        EXPECT_EQ(id, rec.id);
        EXPECT_EQ(rec.vector.size(), 4);
    }

    EXPECT_EQ(found_ids.size(), 15);
}

TEST(RecordIteratorTest, HNSWDatabase_EmptyDatabase) {
    lynx::Config config;
    config.dimension = 4;
    config.index_type = lynx::IndexType::HNSW;
    auto db = lynx::IVectorDatabase::create(config);

    auto records = db->all_records();
    EXPECT_EQ(records.begin(), records.end());
}

TEST(RecordIteratorTest, HNSWDatabase_WithMetadata) {
    lynx::Config config;
    config.dimension = 3;
    config.index_type = lynx::IndexType::HNSW;
    auto db = lynx::IVectorDatabase::create(config);

    // Insert with metadata
    for (int i = 0; i < 5; ++i) {
        std::string metadata = "record_" + std::to_string(i);
        lynx::VectorRecord record{static_cast<uint64_t>(i), {1.0f, 2.0f, 3.0f}, metadata};
        ASSERT_EQ(db->insert(record), lynx::ErrorCode::Ok);
    }

    // Verify metadata is preserved
    for (const auto& [id, rec] : db->all_records()) {
        EXPECT_TRUE(rec.metadata.has_value());
        EXPECT_EQ(rec.metadata.value(), "record_" + std::to_string(id));
    }
}

// ============================================================================
// Thread Safety Tests (basic check for HNSW/IVF)
// ============================================================================

TEST(RecordIteratorTest, IVFDatabase_ThreadSafetyBasic) {
    // This test verifies that the iterator compiles and works
    // with the locking mechanism - full thread safety would require
    // concurrent access tests
    lynx::Config config;
    config.dimension = 4;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 5;
    auto db = lynx::IVectorDatabase::create(config);

    std::vector<lynx::VectorRecord> records;
    for (int i = 0; i < 10; ++i) {
        records.push_back({static_cast<uint64_t>(i), {1.0f, 2.0f, 3.0f, 4.0f}, std::nullopt});
    }
    ASSERT_EQ(db->batch_insert(records), lynx::ErrorCode::Ok);

    // Create iterator - lock is held
    {
        auto all_records = db->all_records();
        size_t count = 0;
        for (const auto& [id, rec] : all_records) {
            count++;
        }
        EXPECT_EQ(count, 10);
    } // Lock released here

    // Can create another iterator after first is destroyed
    {
        auto all_records = db->all_records();
        EXPECT_NE(all_records.begin(), all_records.end());
    }
}

TEST(RecordIteratorTest, HNSWDatabase_ThreadSafetyBasic) {
    lynx::Config config;
    config.dimension = 3;
    config.index_type = lynx::IndexType::HNSW;
    auto db = lynx::IVectorDatabase::create(config);

    for (int i = 0; i < 10; ++i) {
        lynx::VectorRecord record{static_cast<uint64_t>(i), {1.0f, 2.0f, 3.0f}, std::nullopt};
        ASSERT_EQ(db->insert(record), lynx::ErrorCode::Ok);
    }

    // Create iterator - lock is held
    {
        auto all_records = db->all_records();
        size_t count = 0;
        for (const auto& [id, rec] : all_records) {
            count++;
        }
        EXPECT_EQ(count, 10);
    } // Lock released here

    // Can create another iterator after first is destroyed
    {
        auto all_records = db->all_records();
        EXPECT_NE(all_records.begin(), all_records.end());
    }
}
