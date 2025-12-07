/**
 * @file main.cpp
 * @brief Lynx Vector Database - Example Application
 *
 * This file demonstrates basic usage of the Lynx vector database library.
 */

#include "lynx/lynx.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "Lynx Vector Database v" << lynx::IVectorDatabase::version() << "\n";
    std::cout << "========================================\n\n";

    // Display available index types
    std::cout << "Supported Index Types:\n";
    std::cout << "  - " << lynx::index_type_string(lynx::IndexType::Flat) << "\n";
    std::cout << "  - " << lynx::index_type_string(lynx::IndexType::HNSW) << "\n";
    std::cout << "  - " << lynx::index_type_string(lynx::IndexType::IVF) << "\n";
    std::cout << "\n";

    // Display available distance metrics
    std::cout << "Supported Distance Metrics:\n";
    std::cout << "  - " << lynx::distance_metric_string(lynx::DistanceMetric::L2) << "\n";
    std::cout << "  - " << lynx::distance_metric_string(lynx::DistanceMetric::Cosine) << "\n";
    std::cout << "  - " << lynx::distance_metric_string(lynx::DistanceMetric::DotProduct) << "\n";
    std::cout << "\n";

    // Example configuration
    lynx::Config config;
    config.dimension = 128;
    config.index_type = lynx::IndexType::HNSW;
    config.distance_metric = lynx::DistanceMetric::L2;
    config.hnsw_params.m = 16;
    config.hnsw_params.ef_construction = 200;
    config.hnsw_params.ef_search = 50;

    std::cout << "Example Configuration:\n";
    std::cout << "  Dimension: " << config.dimension << "\n";
    std::cout << "  Index Type: " << lynx::index_type_string(config.index_type) << "\n";
    std::cout << "  Distance Metric: " << lynx::distance_metric_string(config.distance_metric) << "\n";
    std::cout << "  HNSW M: " << config.hnsw_params.m << "\n";
    std::cout << "  HNSW ef_construction: " << config.hnsw_params.ef_construction << "\n";
    std::cout << "  HNSW ef_search: " << config.hnsw_params.ef_search << "\n";
    std::cout << "\n";

    // TODO: Create database and perform operations once implemented
    std::cout << "Database operations not yet implemented.\n";
    std::cout << "See CONCEPT.md and STATE.md for implementation roadmap.\n";

    return 0;
}
