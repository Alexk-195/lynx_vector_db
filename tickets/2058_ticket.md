# Ticket 2058: Extract MPS Infrastructure (Keep for Future Use)

**Priority**: Medium
**Created**: 2025-12-14
**Assigned**: Unassigned

## Description

Keep the MPS infrastructure in the codebase for future high-performance use cases. The unified `VectorDatabase` will use simple `std::shared_mutex` threading, while MPS code remains available for future optimization work.

**Note**: Based on recommendations in ticket 2051, we are NOT removing or refactoring MPS. It will remain as-is for potential future use.

## Acceptance Criteria

- [ ] Document MPS components and their purpose:
  - [ ] MPS pools, workers, messages, distributors
  - [ ] VectorDatabase_MPS implementation details
  - [ ] Performance characteristics and use cases
- [ ] Keep all MPS-related files:
  - [ ] `src/lib/vector_database_hnsw.{h,cpp}` (VectorDatabase_MPS)
  - [ ] `src/include/lynx/mps_messages.h`
  - [ ] `src/lib/mps_workers.cpp`
- [ ] Keep MPS tests running in CI/CD
- [ ] Document when to consider using MPS:
  - [ ] Very high concurrency requirements
  - [ ] Need for non-blocking operations
  - [ ] Advanced optimization scenarios
- [ ] Update CONCEPT.md to clarify:
  - [ ] Default: unified VectorDatabase with std::shared_mutex
  - [ ] Advanced: VectorDatabase_MPS for extreme performance needs
  - [ ] MPS available but not required for most use cases

## Notes

**Rationale** (from ticket 2051 recommendations):
- Keep MPS compiling
- Keep tests as MPS will be needed in the future
- MPS provides valuable high-performance path for advanced users
- No need to remove working code

**Documentation Updates**:
- When to use unified VectorDatabase (default, simple)
- When to consider VectorDatabase_MPS (high-performance, complex)
- Migration guide for users who want to switch

**No Code Removal**:
- All existing MPS code stays
- All MPS tests continue to run
- MPS remains a build dependency

## Related Tickets

- Parent: #2051 (Database Architecture Analysis)
- Blocked by: #2057 (Integration testing)
- Related: #2060 (Documentation and migration guide)
- Phase: Phase 3 - MPS Documentation
