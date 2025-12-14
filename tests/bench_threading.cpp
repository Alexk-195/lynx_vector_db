/**
 * @file bench_threading.cpp
 * @brief Performance benchmarks for VectorDatabase threading
 *
 * Run with: ./bin/bench_threading
 */

#include <lynx/lynx.h>
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>

using namespace lynx;
using namespace std::chrono;

// Helper function to convert IndexType to string
std::string index_type_to_string(IndexType type) {
    switch (type) {
        case IndexType::Flat: return "Flat";
        case IndexType::HNSW: return "HNSW";
        case IndexType::IVF: return "IVF";
        default: return "Unknown";
    }
}

struct BenchmarkResult {
    std::string test_name;
    size_t num_threads;
    size_t operations;
    double duration_ms;
    double ops_per_sec;
    double throughput_mbps;
};

void print_header() {
    std::cout << std::left
              << std::setw(35) << "Test"
              << std::setw(10) << "Threads"
              << std::setw(12) << "Operations"
              << std::setw(12) << "Time (ms)"
              << std::setw(15) << "Ops/sec"
              << std::setw(15) << "Throughput"
              << "\n" << std::string(100, '-') << "\n";
}

void print_result(const BenchmarkResult& result) {
    std::cout << std::left
              << std::setw(35) << result.test_name
              << std::setw(10) << result.num_threads
              << std::setw(12) << result.operations
              << std::setw(12) << std::fixed << std::setprecision(2) << result.duration_ms
              << std::setw(15) << std::fixed << std::setprecision(0) << result.ops_per_sec
              << std::setw(15) << std::fixed << std::setprecision(2) << result.throughput_mbps << " MB/s"
              << "\n";
}

// Benchmark concurrent reads (search operations)
BenchmarkResult bench_concurrent_reads(IndexType index_type, size_t dimension,
                                        size_t num_vectors, size_t num_threads,
                                        size_t searches_per_thread) {
    Config config;
    config.dimension = dimension;
    config.index_type = index_type;
    config.hnsw_params.m = 16;
    config.hnsw_params.ef_construction = 200;
    config.ivf_params.n_clusters = std::min(size_t(100), num_vectors / 10);

    auto db = IVectorDatabase::create(config);

    // Insert initial data
    for (size_t i = 0; i < num_vectors; ++i) {
        std::vector<float> vec(dimension);
        for (size_t j = 0; j < dimension; ++j) {
            vec[j] = static_cast<float>(i + j * 0.01);
        }
        db->insert({i, vec, std::nullopt});
    }

    // Benchmark searches
    std::vector<std::thread> threads;
    auto start = high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(t);
            std::uniform_real_distribution<float> dist(0.0f, static_cast<float>(num_vectors));

            for (size_t i = 0; i < searches_per_thread; ++i) {
                std::vector<float> query(dimension);
                for (size_t j = 0; j < dimension; ++j) {
                    query[j] = dist(rng);
                }
                db->search(query, 10);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = high_resolution_clock::now();
    double duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    size_t total_ops = num_threads * searches_per_thread;
    double ops_per_sec = (total_ops / duration_ms) * 1000.0;

    // Throughput = ops/sec * bytes_per_op (query vector) / (1024^2)
    double bytes_per_op = dimension * sizeof(float);
    double throughput_mbps = (ops_per_sec * bytes_per_op) / (1024 * 1024);

    return {
        index_type_to_string(index_type) + " Concurrent Reads",
        num_threads,
        total_ops,
        duration_ms,
        ops_per_sec,
        throughput_mbps
    };
}

// Benchmark concurrent writes (insert operations)
BenchmarkResult bench_concurrent_writes(IndexType index_type, size_t dimension,
                                         size_t num_threads, size_t inserts_per_thread) {
    Config config;
    config.dimension = dimension;
    config.index_type = index_type;
    config.hnsw_params.m = 16;
    config.hnsw_params.ef_construction = 200;
    config.ivf_params.n_clusters = 100;

    auto db = IVectorDatabase::create(config);

    // Benchmark inserts
    std::vector<std::thread> threads;
    auto start = high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(t + 1000);
            std::uniform_real_distribution<float> dist(0.0f, 100.0f);

            for (size_t i = 0; i < inserts_per_thread; ++i) {
                uint64_t id = t * inserts_per_thread + i;
                std::vector<float> vec(dimension);
                for (size_t j = 0; j < dimension; ++j) {
                    vec[j] = dist(rng);
                }
                db->insert({id, vec, std::nullopt});
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = high_resolution_clock::now();
    double duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    size_t total_ops = num_threads * inserts_per_thread;
    double ops_per_sec = (total_ops / duration_ms) * 1000.0;

    // Throughput = ops/sec * bytes_per_op (vector + metadata) / (1024^2)
    double bytes_per_op = sizeof(uint64_t) + dimension * sizeof(float);
    double throughput_mbps = (ops_per_sec * bytes_per_op) / (1024 * 1024);

    return {
        index_type_to_string(index_type) + " Concurrent Writes",
        num_threads,
        total_ops,
        duration_ms,
        ops_per_sec,
        throughput_mbps
    };
}

// Benchmark mixed read/write workload
BenchmarkResult bench_mixed_workload(IndexType index_type, size_t dimension,
                                      size_t initial_vectors, size_t num_threads,
                                      size_t ops_per_thread, double read_ratio) {
    Config config;
    config.dimension = dimension;
    config.index_type = index_type;
    config.hnsw_params.m = 16;
    config.hnsw_params.ef_construction = 200;
    config.ivf_params.n_clusters = std::min(size_t(100), initial_vectors / 10);

    auto db = IVectorDatabase::create(config);

    // Insert initial data
    for (size_t i = 0; i < initial_vectors; ++i) {
        std::vector<float> vec(dimension, static_cast<float>(i));
        db->insert({i, vec, std::nullopt});
    }

    // Benchmark mixed workload
    std::vector<std::thread> threads;
    auto start = high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(t);
            std::uniform_real_distribution<float> ratio_dist(0.0, 1.0);
            std::uniform_real_distribution<float> val_dist(0.0f, 100.0f);
            uint64_t next_id = initial_vectors + t * ops_per_thread;

            for (size_t i = 0; i < ops_per_thread; ++i) {
                if (ratio_dist(rng) < read_ratio) {
                    // Read operation (search)
                    std::vector<float> query(dimension);
                    for (size_t j = 0; j < dimension; ++j) {
                        query[j] = val_dist(rng);
                    }
                    db->search(query, 10);
                } else {
                    // Write operation (insert)
                    std::vector<float> vec(dimension);
                    for (size_t j = 0; j < dimension; ++j) {
                        vec[j] = val_dist(rng);
                    }
                    db->insert({next_id++, vec, std::nullopt});
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = high_resolution_clock::now();
    double duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    size_t total_ops = num_threads * ops_per_thread;
    double ops_per_sec = (total_ops / duration_ms) * 1000.0;

    double bytes_per_op = dimension * sizeof(float);  // Approximate
    double throughput_mbps = (ops_per_sec * bytes_per_op) / (1024 * 1024);

    int read_pct = static_cast<int>(read_ratio * 100);
    std::string test_name = index_type_to_string(index_type) + " Mixed (" +
                             std::to_string(read_pct) + "% read)";

    return {
        test_name,
        num_threads,
        total_ops,
        duration_ms,
        ops_per_sec,
        throughput_mbps
    };
}

// Benchmark scalability (varying thread count)
void bench_scalability(IndexType index_type, size_t dimension, size_t num_vectors) {
    std::cout << "\nScalability Benchmark: " << index_type_to_string(index_type) << "\n";
    print_header();

    std::vector<size_t> thread_counts = {1, 2, 4, 8};
    size_t searches_per_thread = 1000;

    for (size_t num_threads : thread_counts) {
        auto result = bench_concurrent_reads(index_type, dimension, num_vectors,
                                              num_threads, searches_per_thread);
        print_result(result);
    }
}

int main() {
    std::cout << "=== VectorDatabase Threading Performance Benchmarks ===\n\n";

    const size_t dimension = 128;
    const size_t num_vectors = 10000;
    const size_t num_threads = 8;

    // =========================================================================
    // Read Performance (Concurrent Searches)
    // =========================================================================
    std::cout << "\n[1] Read Performance (8 threads, 1000 searches/thread)\n";
    print_header();

    auto flat_read = bench_concurrent_reads(IndexType::Flat, dimension, 1000, num_threads, 1000);
    print_result(flat_read);

    auto hnsw_read = bench_concurrent_reads(IndexType::HNSW, dimension, num_vectors, num_threads, 1000);
    print_result(hnsw_read);

    auto ivf_read = bench_concurrent_reads(IndexType::IVF, dimension, num_vectors, num_threads, 1000);
    print_result(ivf_read);

    // =========================================================================
    // Write Performance (Concurrent Inserts)
    // =========================================================================
    std::cout << "\n[2] Write Performance (8 threads, 500 inserts/thread)\n";
    print_header();

    auto flat_write = bench_concurrent_writes(IndexType::Flat, dimension, num_threads, 500);
    print_result(flat_write);

    auto hnsw_write = bench_concurrent_writes(IndexType::HNSW, dimension, num_threads, 500);
    print_result(hnsw_write);

    auto ivf_write = bench_concurrent_writes(IndexType::IVF, dimension, num_threads, 500);
    print_result(ivf_write);

    // =========================================================================
    // Mixed Workload (90% reads, 10% writes)
    // =========================================================================
    std::cout << "\n[3] Mixed Workload (8 threads, 1000 ops/thread)\n";
    print_header();

    auto flat_mixed = bench_mixed_workload(IndexType::Flat, dimension, 1000, num_threads, 1000, 0.9);
    print_result(flat_mixed);

    auto hnsw_mixed = bench_mixed_workload(IndexType::HNSW, dimension, num_vectors, num_threads, 1000, 0.9);
    print_result(hnsw_mixed);

    auto ivf_mixed = bench_mixed_workload(IndexType::IVF, dimension, num_vectors, num_threads, 1000, 0.9);
    print_result(ivf_mixed);

    // =========================================================================
    // Scalability (1, 2, 4, 8 threads)
    // =========================================================================
    bench_scalability(IndexType::HNSW, dimension, num_vectors);

    std::cout << "\n=== Benchmarks Complete ===\n";
    return 0;
}
