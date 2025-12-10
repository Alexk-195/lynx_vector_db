/**
 * @file test_write_log.cpp
 * @brief Unit tests for WriteLog and non-blocking index optimization
 *
 * These tests verify the write log functionality for non-blocking maintenance
 * operations using the clone-optimize-replay-swap pattern.
 */

#include <gtest/gtest.h>
#include "../src/lib/write_log.h"
#include "../src/lib/vector_database_mps.h"
#include <thread>
#include <vector>
#include <random>

namespace lynx {
namespace testing {

// ============================================================================
// WriteLog Basic Tests
// ============================================================================

TEST(WriteLogTest, InitialState) {
    WriteLog log;

    EXPECT_EQ(log.size(), 0);
    EXPECT_FALSE(log.enabled.load());
    EXPECT_FALSE(log.is_at_warning_threshold());
}

TEST(WriteLogTest, LogInsert) {
    WriteLog log;
    log.enabled.store(true);

    std::vector<float> vector = {1.0f, 2.0f, 3.0f, 4.0f};
    bool result = log.log_insert(42, vector);

    EXPECT_TRUE(result);
    EXPECT_EQ(log.size(), 1);
}

TEST(WriteLogTest, LogRemove) {
    WriteLog log;
    log.enabled.store(true);

    bool result = log.log_remove(42);

    EXPECT_TRUE(result);
    EXPECT_EQ(log.size(), 1);
}

TEST(WriteLogTest, LogMultipleOperations) {
    WriteLog log;
    log.enabled.store(true);

    std::vector<float> vector = {1.0f, 2.0f, 3.0f, 4.0f};

    log.log_insert(1, vector);
    log.log_insert(2, vector);
    log.log_remove(1);
    log.log_insert(3, vector);

    EXPECT_EQ(log.size(), 4);
}

TEST(WriteLogTest, Clear) {
    WriteLog log;
    log.enabled.store(true);

    std::vector<float> vector = {1.0f, 2.0f, 3.0f};

    log.log_insert(1, vector);
    log.log_insert(2, vector);

    EXPECT_EQ(log.size(), 2);

    log.clear();

    EXPECT_EQ(log.size(), 0);
}

TEST(WriteLogTest, PreserveOperationOrder) {
    WriteLog log;

    std::vector<float> vec1 = {1.0f};
    std::vector<float> vec2 = {2.0f};
    std::vector<float> vec3 = {3.0f};

    log.log_insert(1, vec1);
    log.log_remove(2);
    log.log_insert(3, vec3);
    log.log_remove(1);
    log.log_insert(2, vec2);

    // Lock and check order
    std::lock_guard lock(log.mutex);
    ASSERT_EQ(log.entries.size(), 5);

    EXPECT_EQ(log.entries[0].op, WriteLog::Operation::Insert);
    EXPECT_EQ(log.entries[0].id, 1);

    EXPECT_EQ(log.entries[1].op, WriteLog::Operation::Remove);
    EXPECT_EQ(log.entries[1].id, 2);

    EXPECT_EQ(log.entries[2].op, WriteLog::Operation::Insert);
    EXPECT_EQ(log.entries[2].id, 3);

    EXPECT_EQ(log.entries[3].op, WriteLog::Operation::Remove);
    EXPECT_EQ(log.entries[3].id, 1);

    EXPECT_EQ(log.entries[4].op, WriteLog::Operation::Insert);
    EXPECT_EQ(log.entries[4].id, 2);
}

// ============================================================================
// WriteLog Replay Tests
// ============================================================================

TEST(WriteLogTest, ReplayInsertToIndex) {
    WriteLog log;

    // Create a simple HNSW index
    HNSWParams params;
    params.m = 8;
    params.ef_construction = 50;
    HNSWIndex index(4, DistanceMetric::L2, params);

    // Log an insert
    std::vector<float> vector = {1.0f, 2.0f, 3.0f, 4.0f};
    log.log_insert(42, vector);

    // Replay to the index
    log.replay_to(&index);

    // Verify the vector was inserted
    EXPECT_TRUE(index.contains(42));
    EXPECT_EQ(index.size(), 1);
}

TEST(WriteLogTest, ReplayRemoveFromIndex) {
    WriteLog log;

    // Create a simple HNSW index with a vector
    HNSWParams params;
    params.m = 8;
    params.ef_construction = 50;
    HNSWIndex index(4, DistanceMetric::L2, params);

    std::vector<float> vector = {1.0f, 2.0f, 3.0f, 4.0f};
    index.add(42, vector);

    EXPECT_TRUE(index.contains(42));

    // Log a remove
    log.log_remove(42);

    // Replay to the index
    log.replay_to(&index);

    // Verify the vector was removed
    EXPECT_FALSE(index.contains(42));
    EXPECT_EQ(index.size(), 0);
}

TEST(WriteLogTest, ReplayMixedOperations) {
    WriteLog log;

    // Create a simple HNSW index
    HNSWParams params;
    params.m = 8;
    params.ef_construction = 50;
    HNSWIndex index(4, DistanceMetric::L2, params);

    std::vector<float> vec1 = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> vec2 = {5.0f, 6.0f, 7.0f, 8.0f};
    std::vector<float> vec3 = {9.0f, 10.0f, 11.0f, 12.0f};

    // Log operations
    log.log_insert(1, vec1);
    log.log_insert(2, vec2);
    log.log_remove(1);
    log.log_insert(3, vec3);

    // Replay
    log.replay_to(&index);

    // Verify final state
    EXPECT_FALSE(index.contains(1));  // Removed
    EXPECT_TRUE(index.contains(2));   // Still exists
    EXPECT_TRUE(index.contains(3));   // Inserted
    EXPECT_EQ(index.size(), 2);
}

TEST(WriteLogTest, ReplayOverwriteExistingVector) {
    WriteLog log;

    // Create index with an existing vector
    HNSWParams params;
    params.m = 8;
    params.ef_construction = 50;
    HNSWIndex index(4, DistanceMetric::L2, params);

    std::vector<float> vec1 = {1.0f, 1.0f, 1.0f, 1.0f};
    index.add(42, vec1);

    // Log an insert with the same ID but different vector
    std::vector<float> vec2 = {2.0f, 2.0f, 2.0f, 2.0f};
    log.log_insert(42, vec2);

    // Replay should handle the overwrite
    log.replay_to(&index);

    // Verify the vector still exists
    EXPECT_TRUE(index.contains(42));
    EXPECT_EQ(index.size(), 1);
}

// ============================================================================
// WriteLog Thread Safety Tests
// ============================================================================

TEST(WriteLogTest, ConcurrentInserts) {
    WriteLog log;
    log.enabled.store(true);

    const int num_threads = 4;
    const int inserts_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&log, t, inserts_per_thread]() {
            for (int i = 0; i < inserts_per_thread; ++i) {
                std::vector<float> vec = {static_cast<float>(t), static_cast<float>(i)};
                uint64_t id = t * 10000 + i;
                log.log_insert(id, vec);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(log.size(), num_threads * inserts_per_thread);
}

TEST(WriteLogTest, ConcurrentMixedOperations) {
    WriteLog log;
    log.enabled.store(true);

    const int num_threads = 4;
    const int ops_per_thread = 50;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&log, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                if (i % 3 == 0) {
                    log.log_remove(t * 10000 + i);
                } else {
                    std::vector<float> vec = {static_cast<float>(i)};
                    log.log_insert(t * 10000 + i, vec);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(log.size(), num_threads * ops_per_thread);
}

// ============================================================================
// VectorDatabase_MPS Optimize Index Tests
// ============================================================================

TEST(VectorDatabaseMPSTest, OptimizeEmptyDatabase) {
    Config config;
    config.dimension = 128;
    config.num_query_threads = 2;
    config.num_index_threads = 1;

    VectorDatabase_MPS db(config);

    // Optimize an empty database should succeed
    ErrorCode result = db.optimize_index();
    EXPECT_EQ(result, ErrorCode::Ok);
}

TEST(VectorDatabaseMPSTest, OptimizeWithVectors) {
    Config config;
    config.dimension = 4;
    config.num_query_threads = 2;
    config.num_index_threads = 1;

    VectorDatabase_MPS db(config);

    // Insert some vectors
    for (uint64_t i = 0; i < 50; ++i) {
        VectorRecord record;
        record.id = i;
        record.vector = {
            static_cast<float>(i),
            static_cast<float>(i + 1),
            static_cast<float>(i + 2),
            static_cast<float>(i + 3)
        };
        db.insert(record);
    }

    EXPECT_EQ(db.size(), 50);

    // Optimize
    ErrorCode result = db.optimize_index();
    EXPECT_EQ(result, ErrorCode::Ok);

    // Verify all vectors still exist
    EXPECT_EQ(db.size(), 50);
    for (uint64_t i = 0; i < 50; ++i) {
        EXPECT_TRUE(db.contains(i));
    }
}

TEST(VectorDatabaseMPSTest, OptimizeWithConcurrentWrites) {
    Config config;
    config.dimension = 4;
    config.num_query_threads = 2;
    config.num_index_threads = 2;

    VectorDatabase_MPS db(config);

    // Insert initial vectors
    for (uint64_t i = 0; i < 100; ++i) {
        VectorRecord record;
        record.id = i;
        record.vector = {
            static_cast<float>(i),
            static_cast<float>(i + 1),
            static_cast<float>(i + 2),
            static_cast<float>(i + 3)
        };
        db.insert(record);
    }

    // Start optimization in a background thread
    std::thread optimize_thread([&db]() {
        ErrorCode result = db.optimize_index();
        // May return Ok or Busy depending on timing
        EXPECT_TRUE(result == ErrorCode::Ok || result == ErrorCode::Busy);
    });

    // Perform concurrent inserts while optimizing
    for (uint64_t i = 100; i < 150; ++i) {
        VectorRecord record;
        record.id = i;
        record.vector = {
            static_cast<float>(i),
            static_cast<float>(i + 1),
            static_cast<float>(i + 2),
            static_cast<float>(i + 3)
        };
        db.insert(record);
    }

    optimize_thread.join();

    // Verify database consistency - all vectors should be present
    for (uint64_t i = 0; i < 150; ++i) {
        EXPECT_TRUE(db.contains(i)) << "Vector " << i << " not found";
    }
}

TEST(VectorDatabaseMPSTest, WriteLogClearedAfterOptimize) {
    Config config;
    config.dimension = 4;
    config.num_query_threads = 1;
    config.num_index_threads = 1;

    VectorDatabase_MPS db(config);

    // Insert some vectors
    for (uint64_t i = 0; i < 10; ++i) {
        VectorRecord record;
        record.id = i;
        record.vector = {1.0f, 2.0f, 3.0f, 4.0f};
        db.insert(record);
    }

    // Optimize
    ErrorCode result = db.optimize_index();
    EXPECT_EQ(result, ErrorCode::Ok);

    // Check that write log is cleared
    EXPECT_EQ(db.get_write_log().size(), 0);
    EXPECT_FALSE(db.get_write_log().enabled.load());
}

TEST(VectorDatabaseMPSTest, SearchDuringOptimize) {
    Config config;
    config.dimension = 4;
    config.num_query_threads = 2;
    config.num_index_threads = 1;

    VectorDatabase_MPS db(config);

    // Insert vectors
    for (uint64_t i = 0; i < 100; ++i) {
        VectorRecord record;
        record.id = i;
        record.vector = {
            static_cast<float>(i),
            static_cast<float>(i + 1),
            static_cast<float>(i + 2),
            static_cast<float>(i + 3)
        };
        db.insert(record);
    }

    // Perform searches in parallel with optimization
    std::atomic<bool> optimization_done{false};
    std::atomic<int> successful_searches{0};

    std::thread optimize_thread([&db, &optimization_done]() {
        db.optimize_index();
        optimization_done.store(true);
    });

    std::thread search_thread([&db, &optimization_done, &successful_searches]() {
        while (!optimization_done.load()) {
            std::vector<float> query = {50.0f, 51.0f, 52.0f, 53.0f};
            auto result = db.search(query, 5);
            if (!result.items.empty()) {
                successful_searches++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    optimize_thread.join();
    search_thread.join();

    // Searches should have succeeded during optimization
    EXPECT_GT(successful_searches.load(), 0);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST(WriteLogTest, StressTest) {
    WriteLog log;
    log.enabled.store(true);

    const int num_operations = 10000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> op_dist(0, 1);  // 0 = insert, 1 = remove
    std::uniform_int_distribution<uint64_t> id_dist(0, 1000);

    for (int i = 0; i < num_operations; ++i) {
        if (op_dist(gen) == 0) {
            std::vector<float> vec = {static_cast<float>(i)};
            log.log_insert(id_dist(gen), vec);
        } else {
            log.log_remove(id_dist(gen));
        }
    }

    EXPECT_EQ(log.size(), num_operations);

    log.clear();
    EXPECT_EQ(log.size(), 0);
}

} // namespace testing
} // namespace lynx
