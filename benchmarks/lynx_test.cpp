#include "../src/include/lynx/lynx.h"
#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>
#include <vector>

using namespace lynx;

constexpr int NUM_VECTORS = 1'000;
constexpr int DIMENSION = 512;
constexpr int NUM_QUERIES = 10;
constexpr int BATCH_SIZE = 1000;

// Generate random vector
std::vector<float> generate_random_vector(int dim, std::mt19937& gen) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::vector<float> vec(dim);
    for (int i = 0; i < dim; ++i) {
        vec[i] = dist(gen);
    }
    return vec;
}

int main() {
    std::random_device rd;
    std::mt19937 gen(rd());

    // Configure database with HNSW index
    Config config;
    config.dimension = DIMENSION;
    config.index_type = IndexType::HNSW;
    config.distance_metric = DistanceMetric::L2;

    // HNSW parameters to match ChromaDB
    config.hnsw_params.m = 32;                      // graph degree (matches ChromaDB)
    config.hnsw_params.ef_construction = 200;       // ef parameter during construction
    config.hnsw_params.ef_search = 200;             // ef parameter during search

    auto db = IVectorDatabase::create(config);


    std::vector<VectorRecord> all_records;
    all_records.reserve(NUM_VECTORS);
    for (int i = 0; i < NUM_VECTORS; i++) 
    {
        VectorRecord record;
        record.id = static_cast<uint64_t>(i);
        record.vector = generate_random_vector(DIMENSION, gen);
        all_records.push_back(record);
    }

    // Generate data
    std::cout << "Adding vectors in batches..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    db->batch_insert(all_records);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Inserted " << NUM_VECTORS << " vectors in "
              << std::fixed << std::setprecision(2) << elapsed.count()
              << " seconds" << std::endl;

    // Queries
    std::cout << "\nRunning queries..." << std::endl;
    for (int i = 0; i < NUM_QUERIES; ++i) {
        auto query = generate_random_vector(DIMENSION, gen);

        auto t0 = std::chrono::high_resolution_clock::now();
        auto results = db->search(query, 5);
        auto t1 = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> query_time = t1 - t0;

        std::cout << "Query " << (i + 1) << ": "
                  << std::fixed << std::setprecision(4) << query_time.count()
                  << "s, top IDs: [";

        for (size_t j = 0; j < results.items.size(); ++j) {
            std::cout << "'vec_" << results.items[j].id << "'";
            if (j < results.items.size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    return 0;
}
