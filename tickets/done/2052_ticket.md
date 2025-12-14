# Ticket 2052: Implement FlatIndex Class

**Priority**: High
**Created**: 2025-12-14
**Assigned**: Unassigned

## Description

Create a new `FlatIndex` class that implements the `IVectorIndex` interface for brute-force vector search. This establishes architectural consistency with `HNSWIndex` and `IVFIndex`, and extracts the flat search logic from the current `VectorDatabase_Impl` class.

## Acceptance Criteria

- [ ] Create `src/lib/flat_index.h` with `FlatIndex` class declaration
- [ ] Create `src/lib/flat_index.cpp` with implementation
- [ ] Implement all `IVectorIndex` interface methods:
  - [ ] `add(uint64_t id, std::span<const float> vector)`
  - [ ] `remove(uint64_t id)`
  - [ ] `search(std::span<const float> query, size_t k, size_t ef_search)`
  - [ ] `build(std::span<const VectorRecord> vectors)`
  - [ ] `serialize(std::ostream& out)`
  - [ ] `deserialize(std::istream& in)`
- [ ] Extract brute-force search logic from `VectorDatabase_Impl`
- [ ] Support all distance metrics (L2, Cosine, Dot Product)
- [ ] Unit tests for FlatIndex covering:
  - [ ] Basic add/remove operations
  - [ ] Search correctness (all distance metrics)
  - [ ] Build from batch
  - [ ] Serialization/deserialization
  - [ ] Edge cases (empty index, single vector, duplicate IDs)
- [ ] Code coverage >90% for FlatIndex

## Notes

**Implementation Details**:
- Internal storage: `std::unordered_map<uint64_t, std::vector<float>>`
- Search algorithm: Brute-force iteration over all vectors
- Distance calculations: Use centralized `utils::calculate_distance()` functions
- No threading/locking required (single-threaded, managed by database layer)

**Reference Implementation**:
- Extract search logic from `src/lib/vector_database_flat.cpp:search()`
- Follow same pattern as `HNSWIndex` and `IVFIndex`

## Related Tickets

- Parent: #2051 (Database Architecture Analysis)
- Blocks: #2053 (Test FlatIndex integration)
- Phase: Phase 1 - Create FlatIndex
