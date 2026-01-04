#include <faiss/IndexHNSW.h>
#include <faiss/IndexFlat.h>
#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>
#include <vector>
#include <memory>

constexpr int NUM_VECTORS = 10'000;
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

    // Create FAISS HNSW index
    // HNSW parameters to match Lynx benchmark
    constexpr int M = 32;                 // graph degree (matches Lynx)

    std::cout << "Creating FAISS HNSW index..." << std::endl;

    // Create base index (L2 distance metric)
    auto* quantizer = new faiss::IndexFlatL2(DIMENSION);

    // Create HNSW index wrapper
    auto* index = new faiss::IndexHNSWFlat(DIMENSION, M);
    index->hnsw.efConstruction = 200;     // ef parameter during construction (matches Lynx)
    index->hnsw.efSearch = 200;           // ef parameter during search (matches Lynx)

    // Generate all vectors first
    std::cout << "Generating " << NUM_VECTORS << " random vectors..." << std::endl;
    std::vector<float> all_vectors(NUM_VECTORS * DIMENSION);
    for (int i = 0; i < NUM_VECTORS; i++) {
        auto vec = generate_random_vector(DIMENSION, gen);
        std::copy(vec.begin(), vec.end(), all_vectors.begin() + i * DIMENSION);
    }

    // Insert vectors in batches
    std::cout << "Adding vectors in batches..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    index->add(NUM_VECTORS, all_vectors.data());

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Inserted " << NUM_VECTORS << " vectors in "
              << std::fixed << std::setprecision(2) << elapsed.count()
              << " seconds" << std::endl;

    // Queries
    std::cout << "\nRunning queries..." << std::endl;
    constexpr int k = 5;  // number of nearest neighbors
    std::vector<float> distances(k);
    std::vector<faiss::idx_t> labels(k);

    for (int i = 0; i < NUM_QUERIES; ++i) {
        auto query = generate_random_vector(DIMENSION, gen);

        auto t0 = std::chrono::high_resolution_clock::now();
        index->search(1, query.data(), k, distances.data(), labels.data());
        auto t1 = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> query_time = t1 - t0;

        std::cout << "Query " << (i + 1) << ": "
                  << std::fixed << std::setprecision(4) << query_time.count()
                  << "s, top IDs: [";

        for (int j = 0; j < k; ++j) {
            std::cout << "'vec_" << labels[j] << "'";
            if (j < k - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    // Cleanup
    delete index;
    delete quantizer;

    return 0;
}
