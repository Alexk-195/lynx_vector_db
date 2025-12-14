/**
 * @file test_threading.cpp
 * @brief Multi-threaded tests for VectorDatabase thread safety
 */

#include <gtest/gtest.h>
#include <lynx/lynx.h>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>

using namespace lynx;

class ThreadingTest : public ::testing::TestWithParam<IndexType> {
protected:
    void SetUp() override {
        config_.dimension = 128;
        config_.index_type = GetParam();
        config_.hnsw_params.m = 8;
        config_.hnsw_params.ef_construction = 100;
        config_.hnsw_params.ef_search = 50;
        config_.ivf_params.n_clusters = 10;
        config_.ivf_params.n_probe = 3;
    }

    Config config_;
};

// Test concurrent reads (multiple search threads)
TEST_P(ThreadingTest, ConcurrentReads) {
    auto db = IVectorDatabase::create(config_);

    // Insert initial data using batch for IVF efficiency
    const size_t num_vectors = 1000;
    std::vector<VectorRecord> initial_data;
    initial_data.reserve(num_vectors);
    for (size_t i = 0; i < num_vectors; ++i) {
        std::vector<float> vec(config_.dimension);
        for (size_t j = 0; j < config_.dimension; ++j) {
            vec[j] = static_cast<float>(i) + static_cast<float>(j) * 0.01f;
        }
        initial_data.push_back({i, vec, std::nullopt});
    }
    ASSERT_EQ(db->batch_insert(initial_data), ErrorCode::Ok);

    // Launch multiple search threads
    const size_t num_threads = 8;
    std::vector<std::thread> threads;
    std::atomic<size_t> total_searches{0};
    std::atomic<bool> error_occurred{false};

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(t);
            std::uniform_real_distribution<float> dist(0.0f, static_cast<float>(num_vectors));

            for (size_t i = 0; i < 100; ++i) {
                // Create random query
                std::vector<float> query(config_.dimension);
                for (size_t j = 0; j < config_.dimension; ++j) {
                    query[j] = dist(rng);
                }

                // Search
                auto result = db->search(query, 10);
                if (result.items.empty() && db->size() > 0) {
                    error_occurred.store(true);
                }
                total_searches.fetch_add(1);
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_FALSE(error_occurred.load());
    EXPECT_EQ(total_searches.load(), num_threads * 100);
}

// Test concurrent reads and writes
TEST_P(ThreadingTest, ConcurrentReadsAndWrites) {
    auto db = IVectorDatabase::create(config_);

    // Insert initial data using batch
    const size_t initial_vectors = 500;
    std::vector<VectorRecord> initial_data;
    initial_data.reserve(initial_vectors);
    for (size_t i = 0; i < initial_vectors; ++i) {
        std::vector<float> vec(config_.dimension, static_cast<float>(i));
        initial_data.push_back({i, vec, std::nullopt});
    }
    ASSERT_EQ(db->batch_insert(initial_data), ErrorCode::Ok);

    std::atomic<bool> stop{false};
    std::atomic<size_t> insert_count{0};
    std::atomic<size_t> search_count{0};

    // Reader threads (search)
    std::vector<std::thread> reader_threads;
    for (size_t t = 0; t < 4; ++t) {
        reader_threads.emplace_back([&, t]() {
            std::mt19937 rng(t);
            std::uniform_real_distribution<float> dist(0.0f, 100.0f);

            while (!stop.load()) {
                std::vector<float> query(config_.dimension);
                for (size_t j = 0; j < config_.dimension; ++j) {
                    query[j] = dist(rng);
                }
                db->search(query, 5);
                search_count.fetch_add(1);
            }
        });
    }

    // Writer threads (insert)
    std::vector<std::thread> writer_threads;
    std::atomic<uint64_t> next_id{initial_vectors};
    for (size_t t = 0; t < 2; ++t) {
        writer_threads.emplace_back([&, t]() {
            std::mt19937 rng(t + 1000);
            std::uniform_real_distribution<float> dist(0.0f, 100.0f);

            for (size_t i = 0; i < 50; ++i) {
                uint64_t id = next_id.fetch_add(1);
                std::vector<float> vec(config_.dimension);
                for (size_t j = 0; j < config_.dimension; ++j) {
                    vec[j] = dist(rng);
                }
                VectorRecord record{id, vec, std::nullopt};
                db->insert(record);
                insert_count.fetch_add(1);
            }
        });
    }

    // Wait for writers to finish
    for (auto& thread : writer_threads) {
        thread.join();
    }

    // Stop readers
    stop.store(true);
    for (auto& thread : reader_threads) {
        thread.join();
    }

    EXPECT_EQ(insert_count.load(), 100);
    EXPECT_GT(search_count.load(), 0);
    EXPECT_EQ(db->size(), initial_vectors + 100);
}

// Test concurrent writes (serialized by mutex)
TEST_P(ThreadingTest, ConcurrentWrites) {
    auto db = IVectorDatabase::create(config_);

    const size_t num_threads = 8;
    const size_t inserts_per_thread = 50;
    std::vector<std::thread> threads;
    std::atomic<size_t> success_count{0};

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < inserts_per_thread; ++i) {
                uint64_t id = t * inserts_per_thread + i;
                std::vector<float> vec(config_.dimension, static_cast<float>(id));
                VectorRecord record{id, vec, std::nullopt};

                ErrorCode result = db->insert(record);
                if (result == ErrorCode::Ok) {
                    success_count.fetch_add(1);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * inserts_per_thread);
    EXPECT_EQ(db->size(), num_threads * inserts_per_thread);
}

// Test concurrent remove operations
TEST_P(ThreadingTest, ConcurrentRemoves) {
    auto db = IVectorDatabase::create(config_);

    // Insert data using batch
    const size_t num_vectors = 1000;
    std::vector<VectorRecord> initial_data;
    initial_data.reserve(num_vectors);
    for (size_t i = 0; i < num_vectors; ++i) {
        std::vector<float> vec(config_.dimension, static_cast<float>(i));
        initial_data.push_back({i, vec, std::nullopt});
    }
    ASSERT_EQ(db->batch_insert(initial_data), ErrorCode::Ok);

    // Remove half the vectors concurrently
    const size_t num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<size_t> remove_count{0};

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            // Each thread removes non-overlapping IDs
            size_t start = t * (num_vectors / num_threads / 2);
            size_t end = start + (num_vectors / num_threads / 2);

            for (size_t i = start; i < end; ++i) {
                ErrorCode result = db->remove(i);
                if (result == ErrorCode::Ok) {
                    remove_count.fetch_add(1);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(remove_count.load(), num_vectors / 2);
    EXPECT_EQ(db->size(), num_vectors / 2);
}

// Test statistics consistency under concurrent access
TEST_P(ThreadingTest, StatisticsConsistency) {
    auto db = IVectorDatabase::create(config_);

    // For IVF, initialize with at least one vector to avoid concurrent auto-init issues
    if (GetParam() == IndexType::IVF) {
        std::vector<float> init_vec(config_.dimension, 0.0f);
        VectorRecord init_record{999999, init_vec, std::nullopt};
        ASSERT_EQ(db->insert(init_record), ErrorCode::Ok);
    }

    const size_t num_threads = 4;
    const size_t ops_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<size_t> successful_inserts{0};

    // Mix of inserts and searches
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < ops_per_thread; ++i) {
                uint64_t id = t * ops_per_thread + i;
                std::vector<float> vec(config_.dimension, static_cast<float>(id));
                VectorRecord record{id, vec, std::nullopt};
                if (db->insert(record) == ErrorCode::Ok) {
                    successful_inserts.fetch_add(1);
                }

                // Search after every insert
                db->search(vec, 5);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto stats = db->stats();
    size_t expected = num_threads * ops_per_thread;
    if (GetParam() == IndexType::IVF) expected += 1;  // Account for init vector

    EXPECT_EQ(stats.vector_count, expected);
    EXPECT_EQ(successful_inserts.load(), num_threads * ops_per_thread);
    EXPECT_EQ(stats.total_queries, num_threads * ops_per_thread);
}

// Test concurrent batch inserts
TEST_P(ThreadingTest, ConcurrentBatchInserts) {
    auto db = IVectorDatabase::create(config_);

    const size_t num_threads = 4;
    const size_t batch_size = 50;
    std::vector<std::thread> threads;
    std::atomic<size_t> total_inserted{0};

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::vector<VectorRecord> batch;
            for (size_t i = 0; i < batch_size; ++i) {
                uint64_t id = t * batch_size + i;
                std::vector<float> vec(config_.dimension, static_cast<float>(id));
                batch.push_back({id, vec, std::nullopt});
            }

            ErrorCode result = db->batch_insert(batch);
            if (result == ErrorCode::Ok) {
                total_inserted.fetch_add(batch_size);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_inserted.load(), num_threads * batch_size);
    EXPECT_EQ(db->size(), num_threads * batch_size);
}

// Stress test with many threads and operations
TEST_P(ThreadingTest, StressTest) {
    auto db = IVectorDatabase::create(config_);

    // Insert initial data using batch
    const size_t initial_vectors = 200;
    std::vector<VectorRecord> initial_data;
    initial_data.reserve(initial_vectors);
    for (size_t i = 0; i < initial_vectors; ++i) {
        std::vector<float> vec(config_.dimension, static_cast<float>(i));
        initial_data.push_back({i, vec, std::nullopt});
    }
    ASSERT_EQ(db->batch_insert(initial_data), ErrorCode::Ok);

    std::atomic<bool> stop{false};
    std::atomic<size_t> total_ops{0};

    // Mixed workload: readers and writers
    std::vector<std::thread> threads;
    const size_t num_threads = 8;

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(t);
            std::uniform_int_distribution<int> op_dist(0, 9);
            std::uniform_real_distribution<float> val_dist(0.0f, 100.0f);
            std::atomic<uint64_t> local_id{initial_vectors + t * 1000};

            for (size_t i = 0; i < 100; ++i) {
                int op = op_dist(rng);

                if (op < 7) {  // 70% searches
                    std::vector<float> query(config_.dimension);
                    for (size_t j = 0; j < config_.dimension; ++j) {
                        query[j] = val_dist(rng);
                    }
                    db->search(query, 5);
                } else if (op < 9) {  // 20% inserts
                    uint64_t id = local_id.fetch_add(1);
                    std::vector<float> vec(config_.dimension);
                    for (size_t j = 0; j < config_.dimension; ++j) {
                        vec[j] = val_dist(rng);
                    }
                    VectorRecord record{id, vec, std::nullopt};
                    db->insert(record);
                } else {  // 10% stats queries
                    db->stats();
                }

                total_ops.fetch_add(1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(total_ops.load(), num_threads * 100);
    EXPECT_GE(db->size(), initial_vectors);
}

// Parameterized tests for all index types
INSTANTIATE_TEST_SUITE_P(
    AllIndexTypes,
    ThreadingTest,
    ::testing::Values(IndexType::Flat, IndexType::HNSW, IndexType::IVF),
    [](const ::testing::TestParamInfo<ThreadingTest::ParamType>& info) {
        switch (info.param) {
            case IndexType::Flat: return "Flat";
            case IndexType::HNSW: return "HNSW";
            case IndexType::IVF: return "IVF";
            default: return "Unknown";
        }
    }
);
