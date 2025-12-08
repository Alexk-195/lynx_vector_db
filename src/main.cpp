/**
 * @file main.cpp
 * @brief Lynx Vector Database - Example Application
 *
 * This file demonstrates comprehensive usage of the Lynx vector database library,
 * including configuration, CRUD operations, search, batch operations, and persistence.
 */

#include "lynx/lynx.h"
#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <cmath>

// Helper function to generate random vectors
std::vector<float> generate_random_vector(size_t dim, std::mt19937& gen) {
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    std::vector<float> vec(dim);
    for (size_t i = 0; i < dim; ++i) {
        vec[i] = dis(gen);
    }
    return vec;
}

// Helper function to normalize a vector (for cosine similarity)
void normalize_vector(std::vector<float>& vec) {
    float norm = 0.0f;
    for (float v : vec) {
        norm += v * v;
    }
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (float& v : vec) {
            v /= norm;
        }
    }
}

// Helper function to print search results
void print_search_results(const lynx::SearchResult& result, size_t max_display = 5) {
    std::cout << "  Found " << result.items.size() << " results "
              << "(evaluated " << result.total_candidates << " candidates)\n";
    std::cout << "  Query time: " << std::fixed << std::setprecision(3)
              << result.query_time_ms << " ms\n";

    size_t count = std::min(result.items.size(), max_display);
    for (size_t i = 0; i < count; ++i) {
        const auto& item = result.items[i];
        std::cout << "    " << (i + 1) << ". ID=" << item.id
                  << ", Distance=" << std::fixed << std::setprecision(6)
                  << item.distance << "\n";
    }
    if (result.items.size() > max_display) {
        std::cout << "    ... and " << (result.items.size() - max_display)
                  << " more results\n";
    }
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Lynx Vector Database - Usage Example\n";
    std::cout << "Version: " << lynx::IVectorDatabase::version() << "\n";
    std::cout << "========================================\n\n";

    // =========================================================================
    // 1. DISPLAY CAPABILITIES
    // =========================================================================
    std::cout << "1. SUPPORTED FEATURES\n";
    std::cout << "   Index Types:\n";
    std::cout << "     - " << lynx::index_type_string(lynx::IndexType::Flat) << "\n";
    std::cout << "     - " << lynx::index_type_string(lynx::IndexType::HNSW) << "\n";
    std::cout << "     - " << lynx::index_type_string(lynx::IndexType::IVF) << "\n";
    std::cout << "\n   Distance Metrics:\n";
    std::cout << "     - " << lynx::distance_metric_string(lynx::DistanceMetric::L2) << "\n";
    std::cout << "     - " << lynx::distance_metric_string(lynx::DistanceMetric::Cosine) << "\n";
    std::cout << "     - " << lynx::distance_metric_string(lynx::DistanceMetric::DotProduct) << "\n";
    std::cout << "\n";

    // =========================================================================
    // 2. DATABASE CONFIGURATION
    // =========================================================================
    std::cout << "2. DATABASE CONFIGURATION\n";

    lynx::Config config;
    config.dimension = 128;
    config.index_type = lynx::IndexType::HNSW;
    config.distance_metric = lynx::DistanceMetric::L2;
    config.hnsw_params.m = 16;
    config.hnsw_params.ef_construction = 200;
    config.hnsw_params.ef_search = 50;
    config.data_path = "/tmp/lynx_example_db";

    std::cout << "   Dimension: " << config.dimension << "\n";
    std::cout << "   Index Type: " << lynx::index_type_string(config.index_type) << "\n";
    std::cout << "   Distance Metric: " << lynx::distance_metric_string(config.distance_metric) << "\n";
    std::cout << "   HNSW Parameters:\n";
    std::cout << "     - M: " << config.hnsw_params.m << "\n";
    std::cout << "     - ef_construction: " << config.hnsw_params.ef_construction << "\n";
    std::cout << "     - ef_search: " << config.hnsw_params.ef_search << "\n";
    std::cout << "   Data Path: " << config.data_path << "\n";
    std::cout << "\n";

    // =========================================================================
    // 3. CREATE DATABASE
    // =========================================================================
    std::cout << "3. CREATING DATABASE\n";

    auto db = lynx::IVectorDatabase::create(config);
    if (!db) {
        std::cerr << "   ERROR: Failed to create database\n";
        return 1;
    }
    std::cout << "   ✓ Database created successfully\n\n";

    // Initialize random number generator
    std::mt19937 gen(42);  // Fixed seed for reproducibility

    // =========================================================================
    // 4. SINGLE VECTOR INSERT
    // =========================================================================
    std::cout << "4. SINGLE VECTOR INSERT\n";

    // Insert a few individual vectors with metadata
    for (uint64_t id = 1; id <= 5; ++id) {
        lynx::VectorRecord record;
        record.id = id;
        record.vector = generate_random_vector(config.dimension, gen);
        record.metadata = "{\"category\": \"single\", \"id\": " + std::to_string(id) + "}";

        auto err = db->insert(record);
        if (err != lynx::ErrorCode::Ok) {
            std::cerr << "   ERROR: Failed to insert vector " << id
                      << " - " << lynx::error_string(err) << "\n";
        }
    }
    std::cout << "   ✓ Inserted 5 vectors individually\n";
    std::cout << "   Database size: " << db->size() << "\n\n";

    // =========================================================================
    // 5. BATCH INSERT
    // =========================================================================
    std::cout << "5. BATCH INSERT\n";

    // Create a batch of vectors
    std::vector<lynx::VectorRecord> batch;
    for (uint64_t id = 100; id < 1100; ++id) {
        lynx::VectorRecord record;
        record.id = id;
        record.vector = generate_random_vector(config.dimension, gen);
        if (id % 10 == 0) {
            record.metadata = "{\"category\": \"batch\", \"special\": true}";
        }
        batch.push_back(std::move(record));
    }

    auto err = db->batch_insert(batch);
    if (err != lynx::ErrorCode::Ok) {
        std::cerr << "   ERROR: Batch insert failed - " << lynx::error_string(err) << "\n";
    } else {
        std::cout << "   ✓ Batch inserted " << batch.size() << " vectors\n";
    }
    std::cout << "   Database size: " << db->size() << "\n\n";

    // =========================================================================
    // 6. VECTOR RETRIEVAL
    // =========================================================================
    std::cout << "6. VECTOR RETRIEVAL\n";

    // Retrieve a specific vector
    uint64_t test_id = 100;
    auto retrieved = db->get(test_id);
    if (retrieved.has_value()) {
        std::cout << "   ✓ Retrieved vector ID=" << test_id << "\n";
        std::cout << "     Dimension: " << retrieved->vector.size() << "\n";
        if (retrieved->metadata.has_value()) {
            std::cout << "     Metadata: " << *retrieved->metadata << "\n";
        }
    } else {
        std::cout << "   ✗ Vector ID=" << test_id << " not found\n";
    }

    // Check if vectors exist
    std::cout << "   Contains ID=1: " << (db->contains(1) ? "Yes" : "No") << "\n";
    std::cout << "   Contains ID=999: " << (db->contains(999) ? "Yes" : "No") << "\n";
    std::cout << "   Contains ID=9999: " << (db->contains(9999) ? "Yes" : "No") << "\n\n";

    // =========================================================================
    // 7. BASIC SEARCH
    // =========================================================================
    std::cout << "7. BASIC SEARCH (k-NN)\n";

    // Generate a random query vector
    auto query = generate_random_vector(config.dimension, gen);
    std::cout << "   Searching for 10 nearest neighbors...\n";

    auto search_result = db->search(query, 10);
    print_search_results(search_result);
    std::cout << "\n";

    // =========================================================================
    // 8. SEARCH WITH CUSTOM PARAMETERS
    // =========================================================================
    std::cout << "8. SEARCH WITH CUSTOM PARAMETERS\n";

    lynx::SearchParams params;
    params.ef_search = 100;  // Higher ef_search for better recall

    std::cout << "   Searching with ef_search=" << params.ef_search << "...\n";
    auto search_result2 = db->search(query, 10, params);
    print_search_results(search_result2);
    std::cout << "\n";

    // =========================================================================
    // 9. FILTERED SEARCH
    // =========================================================================
    std::cout << "9. FILTERED SEARCH\n";

    // Search only for IDs in a specific range
    lynx::SearchParams filter_params;
    filter_params.filter = [](uint64_t id) {
        return id >= 100 && id < 200;  // Only IDs 100-199
    };

    std::cout << "   Searching with filter (ID: 100-199)...\n";
    auto filtered_result = db->search(query, 10, filter_params);
    print_search_results(filtered_result);
    std::cout << "\n";

    // =========================================================================
    // 10. DATABASE STATISTICS
    // =========================================================================
    std::cout << "10. DATABASE STATISTICS\n";

    auto stats = db->stats();
    std::cout << "   Vector count: " << stats.vector_count << "\n";
    std::cout << "   Dimension: " << stats.dimension << "\n";
    std::cout << "   Memory usage: " << (stats.memory_usage_bytes / 1024.0 / 1024.0)
              << " MB\n";
    std::cout << "   Index memory: " << (stats.index_memory_bytes / 1024.0 / 1024.0)
              << " MB\n";
    std::cout << "   Total queries: " << stats.total_queries << "\n";
    std::cout << "   Total inserts: " << stats.total_inserts << "\n";
    if (stats.total_queries > 0) {
        std::cout << "   Avg query time: " << std::fixed << std::setprecision(3)
                  << stats.avg_query_time_ms << " ms\n";
    }
    std::cout << "\n";

    // =========================================================================
    // 11. PERSISTENCE - SAVE
    // =========================================================================
    std::cout << "11. PERSISTENCE - SAVE TO DISK\n";

    err = db->save();
    if (err != lynx::ErrorCode::Ok) {
        std::cerr << "   ERROR: Failed to save database - "
                  << lynx::error_string(err) << "\n";
    } else {
        std::cout << "   ✓ Database saved to " << config.data_path << "\n";
    }
    std::cout << "\n";

    // =========================================================================
    // 12. PERSISTENCE - LOAD
    // =========================================================================
    std::cout << "12. PERSISTENCE - LOAD FROM DISK\n";

    // Create a new database instance
    auto db2 = lynx::IVectorDatabase::create(config);
    err = db2->load();
    if (err != lynx::ErrorCode::Ok) {
        std::cerr << "   ERROR: Failed to load database - "
                  << lynx::error_string(err) << "\n";
    } else {
        std::cout << "   ✓ Database loaded from " << config.data_path << "\n";
        std::cout << "   Loaded vector count: " << db2->size() << "\n";

        // Verify the loaded database works
        std::cout << "   Verifying loaded database with search...\n";
        auto verify_result = db2->search(query, 5);
        print_search_results(verify_result);
    }
    std::cout << "\n";

    // =========================================================================
    // 13. VECTOR REMOVAL
    // =========================================================================
    std::cout << "13. VECTOR REMOVAL\n";

    // Remove some vectors
    std::vector<uint64_t> ids_to_remove = {1, 2, 3};
    size_t removed_count = 0;
    for (uint64_t id : ids_to_remove) {
        err = db->remove(id);
        if (err == lynx::ErrorCode::Ok) {
            removed_count++;
        }
    }
    std::cout << "   ✓ Removed " << removed_count << " vectors\n";
    std::cout << "   Database size after removal: " << db->size() << "\n";

    // Verify removal
    std::cout << "   Verifying removal:\n";
    std::cout << "     Contains ID=1: " << (db->contains(1) ? "Yes" : "No") << "\n";
    std::cout << "     Contains ID=2: " << (db->contains(2) ? "Yes" : "No") << "\n";
    std::cout << "     Contains ID=4: " << (db->contains(4) ? "Yes" : "No") << "\n";
    std::cout << "\n";

    // =========================================================================
    // 14. DISTANCE METRIC EXAMPLES
    // =========================================================================
    std::cout << "14. DISTANCE METRIC EXAMPLES\n";

    std::vector<float> vec_a = generate_random_vector(config.dimension, gen);
    std::vector<float> vec_b = generate_random_vector(config.dimension, gen);

    float dist_l2 = lynx::distance_l2(vec_a, vec_b);
    float dist_l2_sq = lynx::distance_l2_squared(vec_a, vec_b);

    // Normalize for cosine/dot product
    std::vector<float> vec_a_norm = vec_a;
    std::vector<float> vec_b_norm = vec_b;
    normalize_vector(vec_a_norm);
    normalize_vector(vec_b_norm);

    float dist_cosine = lynx::distance_cosine(vec_a_norm, vec_b_norm);
    float dist_dot = lynx::distance_dot_product(vec_a_norm, vec_b_norm);

    std::cout << "   L2 distance: " << std::fixed << std::setprecision(6)
              << dist_l2 << "\n";
    std::cout << "   L2 squared: " << dist_l2_sq << "\n";
    std::cout << "   Cosine distance (normalized): " << dist_cosine << "\n";
    std::cout << "   Dot product (normalized): " << dist_dot << "\n";
    std::cout << "\n";

    // =========================================================================
    // 15. ERROR HANDLING
    // =========================================================================
    std::cout << "15. ERROR HANDLING EXAMPLES\n";

    // Try to insert a vector with wrong dimension
    lynx::VectorRecord bad_record;
    bad_record.id = 9999;
    bad_record.vector = std::vector<float>(64);  // Wrong dimension!

    err = db->insert(bad_record);
    if (err != lynx::ErrorCode::Ok) {
        std::cout << "   ✓ Correctly rejected wrong dimension: "
                  << lynx::error_string(err) << "\n";
    }

    // Try to search with wrong dimension
    std::vector<float> bad_query(64);  // Wrong dimension!
    auto bad_search = db->search(bad_query, 5);
    if (bad_search.items.empty()) {
        std::cout << "   ✓ Correctly handled bad query dimension\n";
    }

    // Try to remove non-existent vector
    err = db->remove(999999);
    std::cout << "   Remove non-existent vector: "
              << lynx::error_string(err) << "\n";
    std::cout << "\n";

    // =========================================================================
    // SUMMARY
    // =========================================================================
    std::cout << "========================================\n";
    std::cout << "EXAMPLE COMPLETED SUCCESSFULLY\n";
    std::cout << "========================================\n";
    std::cout << "\nFinal Statistics:\n";
    auto final_stats = db->stats();
    std::cout << "  Vectors: " << final_stats.vector_count << "\n";
    std::cout << "  Queries: " << final_stats.total_queries << "\n";
    std::cout << "  Inserts: " << final_stats.total_inserts << "\n";
    std::cout << "  Memory: " << (final_stats.memory_usage_bytes / 1024.0 / 1024.0)
              << " MB\n";
    std::cout << "\nFor more information, see:\n";
    std::cout << "  - README.md for usage guide\n";
    std::cout << "  - CONCEPT.md for architecture details\n";
    std::cout << "  - STATE.md for implementation status\n";
    std::cout << "\n";

    return 0;
}
