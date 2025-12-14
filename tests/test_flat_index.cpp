/**
 * @file test_flat_index.cpp
 * @brief Unit tests for Flat Index implementation
 *
 * Tests for ticket #2052: Implement FlatIndex Class
 *
 * @copyright MIT License
 */

#include "../src/lib/flat_index.h"
#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <sstream>
#include <set>

using namespace lynx;

namespace {

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Generate random vectors for testing.
 */
std::vector<std::vector<float>> generate_random_vectors(
    std::size_t count, std::size_t dimension, std::uint64_t seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<std::vector<float>> vectors;
    vectors.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        std::vector<float> vec(dimension);
        for (std::size_t d = 0; d < dimension; ++d) {
            vec[d] = dist(rng);
        }
        vectors.push_back(std::move(vec));
    }

    return vectors;
}

/**
 * @brief Generate normalized vectors for cosine similarity testing.
 */
std::vector<std::vector<float>> generate_normalized_vectors(
    std::size_t count, std::size_t dimension, std::uint64_t seed = 42) {
    auto vectors = generate_random_vectors(count, dimension, seed);

    // Normalize each vector
    for (auto& vec : vectors) {
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

    return vectors;
}

} // anonymous namespace

// ============================================================================
// Constructor Tests
// ============================================================================

TEST(FlatIndexTest, ConstructorValid) {
    EXPECT_NO_THROW({
        FlatIndex index(64, DistanceMetric::L2);
        EXPECT_EQ(index.dimension(), 64);
        EXPECT_EQ(index.size(), 0);
    });
}

TEST(FlatIndexTest, ConstructorDifferentMetrics) {
    EXPECT_NO_THROW({
        FlatIndex index_l2(64, DistanceMetric::L2);
    });

    EXPECT_NO_THROW({
        FlatIndex index_cosine(64, DistanceMetric::Cosine);
    });

    EXPECT_NO_THROW({
        FlatIndex index_dot(64, DistanceMetric::DotProduct);
    });
}

TEST(FlatIndexTest, ConstructorSmallDimension) {
    EXPECT_NO_THROW({
        FlatIndex index(1, DistanceMetric::L2);
        EXPECT_EQ(index.dimension(), 1);
    });
}

// ============================================================================
// Add Tests
// ============================================================================

TEST(FlatIndexTest, AddSingleVector) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec(8, 1.0f);

    EXPECT_EQ(index.add(1, vec), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 1);
    EXPECT_TRUE(index.contains(1));
}

TEST(FlatIndexTest, AddMultipleVectors) {
    FlatIndex index(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(10, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        EXPECT_EQ(index.add(i, vectors[i]), ErrorCode::Ok);
    }

    EXPECT_EQ(index.size(), 10);
    for (std::size_t i = 0; i < 10; ++i) {
        EXPECT_TRUE(index.contains(i));
    }
}

TEST(FlatIndexTest, AddDimensionMismatch) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec_wrong_dim(4, 1.0f);

    EXPECT_EQ(index.add(1, vec_wrong_dim), ErrorCode::DimensionMismatch);
    EXPECT_EQ(index.size(), 0);
    EXPECT_FALSE(index.contains(1));
}

TEST(FlatIndexTest, AddDuplicateId) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec1(8, 1.0f);
    std::vector<float> vec2(8, 2.0f);

    EXPECT_EQ(index.add(1, vec1), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 1);

    // Adding with same ID should update the vector
    EXPECT_EQ(index.add(1, vec2), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 1);  // Still 1 vector
}

TEST(FlatIndexTest, AddEmptyVector) {
    FlatIndex index(0, DistanceMetric::L2);
    std::vector<float> vec;

    EXPECT_EQ(index.add(1, vec), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 1);
}

// ============================================================================
// Remove Tests
// ============================================================================

TEST(FlatIndexTest, RemoveExistingVector) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec(8, 1.0f);

    index.add(1, vec);
    EXPECT_TRUE(index.contains(1));

    EXPECT_EQ(index.remove(1), ErrorCode::Ok);
    EXPECT_FALSE(index.contains(1));
    EXPECT_EQ(index.size(), 0);
}

TEST(FlatIndexTest, RemoveNonExistingVector) {
    FlatIndex index(8, DistanceMetric::L2);

    EXPECT_EQ(index.remove(999), ErrorCode::VectorNotFound);
}

TEST(FlatIndexTest, RemoveMultipleTimes) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec(8, 1.0f);

    index.add(1, vec);
    EXPECT_EQ(index.remove(1), ErrorCode::Ok);
    // Try to remove again
    EXPECT_EQ(index.remove(1), ErrorCode::VectorNotFound);
}

TEST(FlatIndexTest, RemoveFromMultipleVectors) {
    FlatIndex index(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(10, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    // Remove some vectors
    EXPECT_EQ(index.remove(3), ErrorCode::Ok);
    EXPECT_EQ(index.remove(7), ErrorCode::Ok);

    EXPECT_EQ(index.size(), 8);
    EXPECT_FALSE(index.contains(3));
    EXPECT_FALSE(index.contains(7));
    EXPECT_TRUE(index.contains(0));
    EXPECT_TRUE(index.contains(5));
}

// ============================================================================
// Contains Tests
// ============================================================================

TEST(FlatIndexTest, ContainsExisting) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec(8, 1.0f);

    index.add(1, vec);
    EXPECT_TRUE(index.contains(1));
}

TEST(FlatIndexTest, ContainsNonExisting) {
    FlatIndex index(8, DistanceMetric::L2);
    EXPECT_FALSE(index.contains(999));
}

TEST(FlatIndexTest, ContainsAfterRemove) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec(8, 1.0f);

    index.add(1, vec);
    index.remove(1);
    EXPECT_FALSE(index.contains(1));
}

// ============================================================================
// Search Tests - L2 Distance
// ============================================================================

TEST(FlatIndexTest, SearchL2Empty) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> query(8, 1.0f);

    SearchParams params;
    auto results = index.search(query, 10, params);

    EXPECT_TRUE(results.empty());
}

TEST(FlatIndexTest, SearchL2SingleVector) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec(8, 1.0f);

    index.add(1, vec);

    // Query with identical vector - distance should be 0
    auto results = index.search(vec, 1, SearchParams{});

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, 1);
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-6f);
}

TEST(FlatIndexTest, SearchL2MultipleVectors) {
    FlatIndex index(8, DistanceMetric::L2);

    // Create three vectors at different distances
    std::vector<float> vec1(8, 0.0f);  // Origin
    std::vector<float> vec2(8, 1.0f);  // Distance = sqrt(8)
    std::vector<float> vec3(8, 2.0f);  // Distance = sqrt(32)

    index.add(1, vec1);
    index.add(2, vec2);
    index.add(3, vec3);

    // Query at origin
    std::vector<float> query(8, 0.0f);
    auto results = index.search(query, 3, SearchParams{});

    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0].id, 1);
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-6f);
    EXPECT_EQ(results[1].id, 2);
    EXPECT_GT(results[2].distance, results[1].distance);  // Sorted by distance
}

TEST(FlatIndexTest, SearchL2LimitK) {
    FlatIndex index(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(20, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    std::vector<float> query(8, 0.5f);
    auto results = index.search(query, 5, SearchParams{});

    // Should return exactly k=5 results
    EXPECT_EQ(results.size(), 5);

    // Results should be sorted by distance
    for (std::size_t i = 1; i < results.size(); ++i) {
        EXPECT_LE(results[i-1].distance, results[i].distance);
    }
}

TEST(FlatIndexTest, SearchL2DimensionMismatch) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec(8, 1.0f);
    index.add(1, vec);

    std::vector<float> query_wrong_dim(4, 0.5f);
    auto results = index.search(query_wrong_dim, 10, SearchParams{});

    EXPECT_TRUE(results.empty());
}

// ============================================================================
// Search Tests - Cosine Distance
// ============================================================================

TEST(FlatIndexTest, SearchCosineIdentical) {
    FlatIndex index(8, DistanceMetric::Cosine);
    auto vectors = generate_normalized_vectors(1, 8);

    index.add(1, vectors[0]);

    auto results = index.search(vectors[0], 1, SearchParams{});

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, 1);
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-5f);
}

TEST(FlatIndexTest, SearchCosineMultiple) {
    FlatIndex index(8, DistanceMetric::Cosine);
    auto vectors = generate_normalized_vectors(10, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    auto results = index.search(vectors[0], 5, SearchParams{});

    EXPECT_EQ(results.size(), 5);
    // Results should be sorted by distance
    for (std::size_t i = 1; i < results.size(); ++i) {
        EXPECT_LE(results[i-1].distance, results[i].distance);
    }
}

// ============================================================================
// Search Tests - Dot Product
// ============================================================================

TEST(FlatIndexTest, SearchDotProductIdentical) {
    FlatIndex index(8, DistanceMetric::DotProduct);
    std::vector<float> vec(8, 1.0f);

    index.add(1, vec);

    auto results = index.search(vec, 1, SearchParams{});

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, 1);
    // Dot product of vec with itself = 8.0, negated = -8.0
    EXPECT_NEAR(results[0].distance, -8.0f, 1e-5f);
}

TEST(FlatIndexTest, SearchDotProductMultiple) {
    FlatIndex index(8, DistanceMetric::DotProduct);
    auto vectors = generate_random_vectors(10, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    std::vector<float> query(8, 1.0f);
    auto results = index.search(query, 5, SearchParams{});

    EXPECT_EQ(results.size(), 5);
    // Results should be sorted (smaller/more negative is better)
    for (std::size_t i = 1; i < results.size(); ++i) {
        EXPECT_LE(results[i-1].distance, results[i].distance);
    }
}

// ============================================================================
// Search Tests - Filtering
// ============================================================================

TEST(FlatIndexTest, SearchWithFilter) {
    FlatIndex index(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(10, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    // Filter: only include even IDs
    SearchParams params;
    params.filter = [](std::uint64_t id) {
        return id % 2 == 0;
    };

    std::vector<float> query(8, 0.5f);
    auto results = index.search(query, 10, params);

    // Should have at most 5 results (IDs 0, 2, 4, 6, 8)
    EXPECT_LE(results.size(), 5);

    // Verify all returned IDs are even
    for (const auto& item : results) {
        EXPECT_EQ(item.id % 2, 0);
    }
}

TEST(FlatIndexTest, SearchWithFilterNoMatches) {
    FlatIndex index(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(5, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);  // IDs 0-4
    }

    // Filter: only include IDs >= 100
    SearchParams params;
    params.filter = [](std::uint64_t id) {
        return id >= 100;
    };

    std::vector<float> query(8, 0.5f);
    auto results = index.search(query, 10, params);

    EXPECT_TRUE(results.empty());
}

// ============================================================================
// Build Tests
// ============================================================================

TEST(FlatIndexTest, BuildFromEmpty) {
    FlatIndex index(8, DistanceMetric::L2);

    std::vector<VectorRecord> records;
    EXPECT_EQ(index.build(records), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 0);
}

TEST(FlatIndexTest, BuildFromBatch) {
    FlatIndex index(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(10, 8);

    std::vector<VectorRecord> records;
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    EXPECT_EQ(index.build(records), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 10);

    for (std::size_t i = 0; i < 10; ++i) {
        EXPECT_TRUE(index.contains(i));
    }
}

TEST(FlatIndexTest, BuildClearsExistingData) {
    FlatIndex index(8, DistanceMetric::L2);

    // Add some vectors
    std::vector<float> vec(8, 1.0f);
    index.add(1, vec);
    index.add(2, vec);
    EXPECT_EQ(index.size(), 2);

    // Build with new data
    auto vectors = generate_random_vectors(5, 8);
    std::vector<VectorRecord> records;
    for (std::size_t i = 100; i < 105; ++i) {
        records.push_back({i, vectors[i - 100], std::nullopt});
    }

    EXPECT_EQ(index.build(records), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 5);
    EXPECT_FALSE(index.contains(1));
    EXPECT_FALSE(index.contains(2));
    EXPECT_TRUE(index.contains(100));
}

TEST(FlatIndexTest, BuildDimensionMismatch) {
    FlatIndex index(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(5, 4);  // Wrong dimension

    std::vector<VectorRecord> records;
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        records.push_back({i, vectors[i], std::nullopt});
    }

    EXPECT_EQ(index.build(records), ErrorCode::DimensionMismatch);
    EXPECT_EQ(index.size(), 0);  // Should be cleared on error
}

// ============================================================================
// Serialization Tests
// ============================================================================

TEST(FlatIndexTest, SerializeEmpty) {
    FlatIndex index(8, DistanceMetric::L2);

    std::ostringstream oss;
    EXPECT_EQ(index.serialize(oss), ErrorCode::Ok);

    // Verify some data was written
    EXPECT_GT(oss.str().size(), 0);
}

TEST(FlatIndexTest, SerializeAndDeserialize) {
    FlatIndex index1(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(10, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index1.add(i, vectors[i]);
    }

    // Serialize
    std::ostringstream oss;
    EXPECT_EQ(index1.serialize(oss), ErrorCode::Ok);

    // Deserialize into new index
    FlatIndex index2(8, DistanceMetric::L2);
    std::istringstream iss(oss.str());
    EXPECT_EQ(index2.deserialize(iss), ErrorCode::Ok);

    // Verify
    EXPECT_EQ(index2.size(), index1.size());
    EXPECT_EQ(index2.dimension(), index1.dimension());

    for (std::size_t i = 0; i < 10; ++i) {
        EXPECT_TRUE(index2.contains(i));
    }

    // Search should give same results
    std::vector<float> query(8, 0.5f);
    auto results1 = index1.search(query, 5, SearchParams{});
    auto results2 = index2.search(query, 5, SearchParams{});

    ASSERT_EQ(results1.size(), results2.size());
    for (std::size_t i = 0; i < results1.size(); ++i) {
        EXPECT_EQ(results1[i].id, results2[i].id);
        EXPECT_NEAR(results1[i].distance, results2[i].distance, 1e-5f);
    }
}

TEST(FlatIndexTest, DeserializeDimensionMismatch) {
    FlatIndex index1(8, DistanceMetric::L2);
    std::vector<float> vec(8, 1.0f);
    index1.add(1, vec);

    std::ostringstream oss;
    index1.serialize(oss);

    // Try to deserialize into index with different dimension
    FlatIndex index2(16, DistanceMetric::L2);
    std::istringstream iss(oss.str());
    EXPECT_EQ(index2.deserialize(iss), ErrorCode::DimensionMismatch);
    EXPECT_EQ(index2.size(), 0);  // Should be empty on error
}

TEST(FlatIndexTest, DeserializeMetricMismatch) {
    FlatIndex index1(8, DistanceMetric::L2);
    std::vector<float> vec(8, 1.0f);
    index1.add(1, vec);

    std::ostringstream oss;
    index1.serialize(oss);

    // Try to deserialize into index with different metric
    FlatIndex index2(8, DistanceMetric::Cosine);
    std::istringstream iss(oss.str());
    EXPECT_EQ(index2.deserialize(iss), ErrorCode::InvalidParameter);
    EXPECT_EQ(index2.size(), 0);  // Should be empty on error
}

TEST(FlatIndexTest, DeserializeInvalidMagicNumber) {
    FlatIndex index(8, DistanceMetric::L2);

    std::ostringstream oss;
    oss << "invalid data";

    std::istringstream iss(oss.str());
    EXPECT_EQ(index.deserialize(iss), ErrorCode::IOError);
}

TEST(FlatIndexTest, DeserializeInvalidVersion) {
    FlatIndex index(8, DistanceMetric::L2);

    std::ostringstream oss;
    // Write valid magic number but invalid version
    std::uint32_t magic = 0x464C4154;  // "FLAT"
    std::uint32_t version = 999;  // Invalid version
    oss.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    oss.write(reinterpret_cast<const char*>(&version), sizeof(version));

    std::istringstream iss(oss.str());
    EXPECT_EQ(index.deserialize(iss), ErrorCode::IOError);
    EXPECT_EQ(index.size(), 0);  // Should remain empty
}

TEST(FlatIndexTest, SerializeDifferentMetrics) {
    for (auto metric : {DistanceMetric::L2, DistanceMetric::Cosine, DistanceMetric::DotProduct}) {
        FlatIndex index1(8, metric);
        std::vector<float> vec(8, 1.0f);
        index1.add(1, vec);

        std::ostringstream oss;
        EXPECT_EQ(index1.serialize(oss), ErrorCode::Ok);

        FlatIndex index2(8, metric);
        std::istringstream iss(oss.str());
        EXPECT_EQ(index2.deserialize(iss), ErrorCode::Ok);

        EXPECT_EQ(index2.size(), 1);
        EXPECT_TRUE(index2.contains(1));
    }
}

// ============================================================================
// Properties Tests
// ============================================================================

TEST(FlatIndexTest, SizeProperty) {
    FlatIndex index(8, DistanceMetric::L2);
    EXPECT_EQ(index.size(), 0);

    std::vector<float> vec(8, 1.0f);
    index.add(1, vec);
    EXPECT_EQ(index.size(), 1);

    index.add(2, vec);
    EXPECT_EQ(index.size(), 2);

    index.remove(1);
    EXPECT_EQ(index.size(), 1);
}

TEST(FlatIndexTest, DimensionProperty) {
    FlatIndex index(64, DistanceMetric::L2);
    EXPECT_EQ(index.dimension(), 64);

    // Dimension should remain constant
    std::vector<float> vec(64, 1.0f);
    index.add(1, vec);
    EXPECT_EQ(index.dimension(), 64);
}

TEST(FlatIndexTest, MemoryUsage) {
    FlatIndex index(8, DistanceMetric::L2);

    std::size_t initial_memory = index.memory_usage();
    EXPECT_GT(initial_memory, 0);

    // Add vectors and check memory increases
    auto vectors = generate_random_vectors(100, 8);
    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    std::size_t memory_after = index.memory_usage();
    EXPECT_GT(memory_after, initial_memory);

    // Memory usage should be proportional to number of vectors
    EXPECT_GT(memory_after, 100 * 8 * sizeof(float));
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(FlatIndexTest, EdgeCaseZeroDimension) {
    FlatIndex index(0, DistanceMetric::L2);
    std::vector<float> vec;

    EXPECT_EQ(index.add(1, vec), ErrorCode::Ok);
    EXPECT_EQ(index.size(), 1);

    auto results = index.search(vec, 10, SearchParams{});
    EXPECT_EQ(results.size(), 1);
}

TEST(FlatIndexTest, EdgeCaseSingleVectorDimension) {
    FlatIndex index(1, DistanceMetric::L2);
    std::vector<float> vec1 = {1.0f};
    std::vector<float> vec2 = {2.0f};

    index.add(1, vec1);
    index.add(2, vec2);

    std::vector<float> query = {1.5f};
    auto results = index.search(query, 2, SearchParams{});

    ASSERT_EQ(results.size(), 2);
    // Both vectors are equidistant from query (distance 0.5)
    EXPECT_NEAR(results[0].distance, 0.5f, 1e-5f);
    EXPECT_NEAR(results[1].distance, 0.5f, 1e-5f);
    // Verify both IDs are present (order doesn't matter when distances are equal)
    std::set<std::uint64_t> ids = {results[0].id, results[1].id};
    EXPECT_EQ(ids.size(), 2);
    EXPECT_TRUE(ids.find(1) != ids.end());
    EXPECT_TRUE(ids.find(2) != ids.end());
}

TEST(FlatIndexTest, EdgeCaseLargeK) {
    FlatIndex index(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(10, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    std::vector<float> query(8, 0.5f);
    // Request more results than available
    auto results = index.search(query, 100, SearchParams{});

    // Should return all 10 vectors
    EXPECT_EQ(results.size(), 10);
}

TEST(FlatIndexTest, EdgeCaseKEqualsZero) {
    FlatIndex index(8, DistanceMetric::L2);
    auto vectors = generate_random_vectors(10, 8);

    for (std::size_t i = 0; i < vectors.size(); ++i) {
        index.add(i, vectors[i]);
    }

    std::vector<float> query(8, 0.5f);
    auto results = index.search(query, 0, SearchParams{});

    EXPECT_TRUE(results.empty());
}

TEST(FlatIndexTest, EdgeCaseDuplicateIdWithDifferentVectors) {
    FlatIndex index(8, DistanceMetric::L2);
    std::vector<float> vec1(8, 1.0f);
    std::vector<float> vec2(8, 2.0f);

    index.add(1, vec1);
    EXPECT_EQ(index.size(), 1);

    // Add same ID with different vector
    index.add(1, vec2);
    EXPECT_EQ(index.size(), 1);

    // Search should use the updated vector
    std::vector<float> query(8, 2.0f);
    auto results = index.search(query, 1, SearchParams{});

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].id, 1);
    EXPECT_NEAR(results[0].distance, 0.0f, 1e-5f);  // Should match vec2
}
