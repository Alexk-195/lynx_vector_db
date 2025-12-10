/**
 * @file test_lynx_coverage.cpp
 * @brief Coverage tests for lynx.h virtual destructors
 *
 * This test file specifically targets the complete object destructors (D0Ev)
 * for IVectorIndex and IVectorDatabase to achieve 100% function coverage.
 *
 * The coverage tool found 4 functions in lynx.h but only 2 were hit by tests.
 * The missing functions are the complete object destructors that are only
 * called when using delete on raw base class pointers (not std::shared_ptr).
 */

#include <gtest/gtest.h>
#include "lynx/lynx.h"

namespace lynx {

// ============================================================================
// Mock Implementations for Coverage Testing
// ============================================================================

/**
 * @brief Minimal concrete implementation of IVectorIndex for coverage testing.
 *
 * This class provides minimal implementations of all pure virtual methods
 * to allow instantiation and deletion via base class pointer.
 */
class MockVectorIndex : public IVectorIndex {
public:
    virtual ~MockVectorIndex() override = default;

    // Vector Operations
    ErrorCode add(std::uint64_t id, std::span<const float> vector) override {
        return ErrorCode::Ok;
    }

    ErrorCode remove(std::uint64_t id) override {
        return ErrorCode::Ok;
    }

    bool contains(std::uint64_t id) const override {
        return false;
    }

    // Search Operations
    std::vector<SearchResultItem> search(
        std::span<const float> query,
        std::size_t k,
        const SearchParams& params) const override {
        return {};
    }

    // Batch Operations
    ErrorCode build(std::span<const VectorRecord> vectors) override {
        return ErrorCode::Ok;
    }

    // Serialization
    ErrorCode serialize(std::ostream& out) const override {
        return ErrorCode::Ok;
    }

    ErrorCode deserialize(std::istream& in) override {
        return ErrorCode::Ok;
    }

    // Properties
    std::size_t size() const override {
        return 0;
    }

    std::size_t dimension() const override {
        return 128;
    }

    std::size_t memory_usage() const override {
        return 0;
    }
};

/**
 * @brief Minimal concrete implementation of IVectorDatabase for coverage testing.
 *
 * This class provides minimal implementations of all pure virtual methods
 * to allow instantiation and deletion via base class pointer.
 */
class MockVectorDatabase : public IVectorDatabase {
public:
    virtual ~MockVectorDatabase() override = default;

    // Single Vector Operations
    ErrorCode insert(const VectorRecord& record) override {
        return ErrorCode::Ok;
    }

    ErrorCode remove(std::uint64_t id) override {
        return ErrorCode::Ok;
    }

    bool contains(std::uint64_t id) const override {
        return false;
    }

    std::optional<VectorRecord> get(std::uint64_t id) const override {
        return std::nullopt;
    }

    // Search Operations
    SearchResult search(std::span<const float> query, std::size_t k) const override {
        return SearchResult{};
    }

    SearchResult search(
        std::span<const float> query,
        std::size_t k,
        const SearchParams& params) const override {
        return SearchResult{};
    }

    // Batch Operations
    ErrorCode batch_insert(std::span<const VectorRecord> records) override {
        return ErrorCode::Ok;
    }

    // Database Properties
    std::size_t size() const override {
        return 0;
    }

    std::size_t dimension() const override {
        return 128;
    }

    DatabaseStats stats() const override {
        return DatabaseStats{};
    }

    const Config& config() const override {
        static Config default_config;
        return default_config;
    }

    // Persistence
    ErrorCode flush() override {
        return ErrorCode::Ok;
    }

    ErrorCode save() override {
        return ErrorCode::Ok;
    }

    ErrorCode load() override {
        return ErrorCode::Ok;
    }
};

} // namespace lynx

// ============================================================================
// Coverage Tests
// ============================================================================

/**
 * @brief Test to invoke IVectorIndex complete object destructor (D0Ev).
 *
 * This test explicitly invokes the complete object destructor variant
 * by allocating a derived class and deleting through a base class pointer.
 * This is the only way to trigger the D0Ev destructor variant.
 */
TEST(LynxCoverageTest, VectorIndex_D0Ev_Coverage) {
    // Allocate the derived class object on the heap
    // and cast to base class pointer
    lynx::IVectorIndex* raw_ptr = new lynx::MockVectorIndex();

    // Call delete on the base class pointer
    // This explicitly invokes the IVectorIndex::D0Ev (complete object destructor)
    delete raw_ptr;
}

/**
 * @brief Test to invoke IVectorDatabase complete object destructor (D0Ev).
 *
 * This test explicitly invokes the complete object destructor variant
 * by allocating a derived class and deleting through a base class pointer.
 * This is the only way to trigger the D0Ev destructor variant.
 */
TEST(LynxCoverageTest, VectorDatabase_D0Ev_Coverage) {
    // Allocate the derived class object on the heap
    // and cast to base class pointer
    lynx::IVectorDatabase* raw_ptr = new lynx::MockVectorDatabase();

    // Call delete on the base class pointer
    // This explicitly invokes the IVectorDatabase::D0Ev (complete object destructor)
    delete raw_ptr;
}
