# Ticket: Create IVFIndex Class Structure and Basic Operations

*Important*: Read README.md and tickets/README.md if not yet done.

**Priority**: High
**Created**: 2025-12-12
**Assigned**: Unassigned

## Description

Create the IVFIndex class structure implementing the IVectorIndex interface. This ticket focuses on establishing the class foundation, data structures, and basic operations (add, contains) needed for IVF functionality.

IVF (Inverted File Index) partitions the vector space into clusters, storing vectors in "inverted lists" associated with each cluster centroid. This structure enables efficient approximate search by limiting queries to a subset of clusters.

## Acceptance Criteria

- [ ] Create header file `src/lib/ivf_index.h`
- [ ] Create implementation file `src/lib/ivf_index.cpp`
- [ ] Define IVFIndex class implementing IVectorIndex interface
- [ ] Implement data structures:
  - [ ] Centroids storage (vector of vectors)
  - [ ] Inverted lists (map from cluster_id to vector of (id, vector) pairs)
  - [ ] ID-to-cluster mapping for fast lookups
- [ ] Implement constructor with configuration
- [ ] Implement `add()` method
  - [ ] Find nearest cluster centroid
  - [ ] Add vector to appropriate inverted list
  - [ ] Update ID-to-cluster mapping
- [ ] Implement `contains()` method
  - [ ] Check if vector ID exists in index
- [ ] Implement `size()` method
- [ ] Implement `dimension()` method
- [ ] Implement `memory_usage()` method
- [ ] Add thread-safety with std::shared_mutex (read-write lock)
- [ ] Add basic error handling for dimension mismatches
- [ ] Create initial unit tests in `tests/test_ivf_index.cpp`
  - [ ] Test construction with various parameters
  - [ ] Test add() with single vectors
  - [ ] Test contains() functionality
  - [ ] Test dimension validation

## Notes

### Implementation Guidelines

1. **Class Structure** (following HNSW pattern):
   ```cpp
   // ivf_index.h
   namespace lynx {

   class IVFIndex : public IVectorIndex {
   public:
       IVFIndex(std::size_t dimension, DistanceMetric metric, const IVFParams& params);
       ~IVFIndex() override = default;

       // IVectorIndex interface
       ErrorCode add(std::uint64_t id, std::span<const float> vector) override;
       ErrorCode remove(std::uint64_t id) override;  // Stub for now
       bool contains(std::uint64_t id) const override;

       std::vector<SearchResultItem> search(
           std::span<const float> query,
           std::size_t k,
           const SearchParams& params) const override;  // Stub for now

       ErrorCode build(std::span<const VectorRecord> vectors) override;  // Stub for now
       ErrorCode serialize(std::ostream& out) const override;  // Stub for now
       ErrorCode deserialize(std::istream& in) override;  // Stub for now

       std::size_t size() const override;
       std::size_t dimension() const override;
       std::size_t memory_usage() const override;

   private:
       // Data structures
       struct InvertedList {
           std::vector<std::uint64_t> ids;
           std::vector<std::vector<float>> vectors;
       };

       std::size_t dimension_;
       DistanceMetric metric_;
       IVFParams params_;

       std::vector<std::vector<float>> centroids_;  // k centroids
       std::vector<InvertedList> inverted_lists_;   // k inverted lists
       std::unordered_map<std::uint64_t, std::size_t> id_to_cluster_;  // id -> cluster mapping

       mutable std::shared_mutex mutex_;  // Thread safety

       // Helper methods
       std::size_t find_nearest_centroid(std::span<const float> vector) const;
       float calculate_distance(std::span<const float> a, std::span<const float> b) const;
   };

   } // namespace lynx
   ```

2. **Data Structure Design**:
   - **Centroids**: Store k centroids as vectors
   - **Inverted Lists**: One list per cluster, storing vector IDs and data
   - **ID Mapping**: Fast lookup to find which cluster contains a given ID

3. **add() Implementation**:
   ```cpp
   ErrorCode IVFIndex::add(std::uint64_t id, std::span<const float> vector) {
       // Validate dimension
       if (vector.size() != dimension_) {
           return ErrorCode::DimensionMismatch;
       }

       // Check if already exists
       if (contains(id)) {
           return ErrorCode::InvalidState;  // ID already exists
       }

       // Find nearest centroid
       std::size_t cluster_id = find_nearest_centroid(vector);

       // Add to inverted list
       std::unique_lock lock(mutex_);
       inverted_lists_[cluster_id].ids.push_back(id);
       inverted_lists_[cluster_id].vectors.push_back(
           std::vector<float>(vector.begin(), vector.end()));
       id_to_cluster_[id] = cluster_id;

       return ErrorCode::Ok;
   }
   ```

4. **Thread Safety**:
   - Use std::shared_mutex for concurrent reads
   - std::shared_lock for read operations (search, contains)
   - std::unique_lock for write operations (add, remove)

5. **Memory Optimization**:
   - Store vectors efficiently in inverted lists
   - Consider reserving capacity for inverted lists based on expected distribution

### Testing Strategy

1. **Basic Operations**:
   - Create index with known centroids
   - Add vectors and verify they're in correct clusters
   - Test contains() for existing and non-existing IDs

2. **Edge Cases**:
   - Add to empty index (should fail - no centroids)
   - Add vector with wrong dimension
   - Add duplicate ID

3. **Thread Safety**:
   - Concurrent reads (multiple contains() calls)
   - Verify no race conditions with read-write patterns

## Related Tickets

- Parent: #2000 (Implement IVF)
- Blocked by: #2001 (K-Means clustering)
- Blocks: #2003 (IVF search implementation)
