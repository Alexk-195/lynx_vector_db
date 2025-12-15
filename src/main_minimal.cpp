/**
 * @file main_minimal.cpp
 * @brief Minimal example showing basic Lynx vector database usage
 *
 * This demonstrates the simplest possible workflow:
 * 1. Create database
 * 2. Insert vectors
 * 3. Search for nearest neighbors
 * 
 * After installing Lynx with ./setup.sh install execute:
 * g++ -o lynx_minimal main_minimal.cpp -std=c++20 -llynx
 * to compite this example.
 */

#include "lynx/lynx.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "Lynx Minimal Example\n";
    std::cout << "====================\n\n";

    // 1. Configure database
    lynx::Config config;
    config.dimension = 4;  // Small dimension for simplicity
    config.index_type = lynx::IndexType::HNSW;
    config.distance_metric = lynx::DistanceMetric::L2;

    std::cout << "Configuration:\n";
    std::cout << "  Dimension: " << config.dimension << "\n";
    std::cout << "  Index: " << lynx::index_type_string(config.index_type) << "\n";
    std::cout << "  Metric: " << lynx::distance_metric_string(config.distance_metric) << "\n\n";

    // 2. Create database
    // The factory method creates a unified VectorDatabase that works with
    // all index types (Flat, HNSW, IVF) using std::shared_mutex for thread safety.
    auto db = lynx::IVectorDatabase::create(config);
    if (!db) {
        std::cerr << "ERROR: Failed to create database\n";
        return 1;
    }
    std::cout << "Database created (using unified VectorDatabase)\n\n";

    // 3. Insert vectors
    std::cout << "Inserting vectors:\n";

    lynx::VectorRecord rec1;
    rec1.id = 1;
    rec1.vector = {1.0f, 0.0f, 0.0f, 0.0f};
    db->insert(rec1);
    std::cout << "  Inserted vector 1: [1, 0, 0, 0]\n";

    lynx::VectorRecord rec2;
    rec2.id = 2;
    rec2.vector = {0.0f, 1.0f, 0.0f, 0.0f};
    db->insert(rec2);
    std::cout << "  Inserted vector 2: [0, 1, 0, 0]\n";

    lynx::VectorRecord rec3;
    rec3.id = 3;
    rec3.vector = {0.9f, 0.1f, 0.0f, 0.0f};
    db->insert(rec3);
    std::cout << "  Inserted vector 3: [0.9, 0.1, 0, 0]\n";

    std::cout << "\nDatabase size: " << db->size() << " vectors\n\n";

    // 4. Search for nearest neighbors
    std::vector<float> query = {1.0f, 0.0f, 0.0f, 0.0f};
    std::cout << "Searching for 2 nearest neighbors to [1, 0, 0, 0]:\n";

    auto results = db->search(query, 2);

    std::cout << "Results:\n";
    for (const auto& item : results.items) {
        std::cout << "  ID " << item.id << ": distance = " << item.distance << "\n";
    }

    std::cout << "\n✓ Example completed successfully\n";

    return 0;
}
