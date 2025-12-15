# Ticket 2061: Remove Old Database Implementations

**Priority**: High
**Created**: 2025-12-14
**Assigned**: Unassigned

## Description

Remove the old database implementations (`VectorDatabase_MPS`, `VectorDatabase_Impl` and `VectorDatabase_IVF`) now that the unified `VectorDatabase` class is complete, tested, and documented. 

**Note**: Based on ticket 2051 recommendations, no backward compatibility is required. We will cleanly remove the old implementations. Also in opposite to previous information the decision was now met to remove VectorDatabase_MPS completely.



## Acceptance Criteria

- [ ] Remove VectorDatabase_Impl (Flat):
  - [ ] Delete `src/lib/vector_database_flat.h`
  - [ ] Delete `src/lib/vector_database_flat.cpp`
  - [ ] Remove from build system (CMakeLists.txt, Makefile)
  - [ ] Remove associated tests (if separate from new tests)
- [ ] Remove VectorDatabase_IVF:
  - [ ] Delete `src/lib/vector_database_ivf.h`
  - [ ] Delete `src/lib/vector_database_ivf.cpp`
  - [ ] Remove from build system (CMakeLists.txt, Makefile)
  - [ ] Remove associated tests (if separate from new tests)
- [ ] Keep VectorDatabase_MPS:
  - [ ] Keep `src/lib/vector_database_hnsw.{h,cpp}`
  - [ ] Keep MPS tests
  - [ ] Document it's for advanced use cases
- [ ] Update factory in `src/lib/lynx.cpp`:
  - [ ] Remove branching logic
  - [ ] Default to unified VectorDatabase
  - [ ] Keep option for VectorDatabase_MPS if needed
- [ ] Update include paths and headers:
  - [ ] Remove old includes from public headers
  - [ ] Clean up internal includes
- [ ] Verify all tests still pass:
  - [ ] No broken tests
  - [ ] No missing functionality
  - [ ] Coverage maintained >85%
- [ ] Clean up build artifacts:
  - [ ] Remove any orphaned object files
  - [ ] Update build scripts
  - [ ] Clean rebuild test

## Notes

**Files to Delete**:
```
src/lib/vector_database_flat.h
src/lib/vector_database_flat.cpp
src/lib/vector_database_ivf.h
src/lib/vector_database_ivf.cpp
```

**Files to Keep**:
```
src/lib/vector_database_hnsw.h   (VectorDatabase_MPS)
src/lib/vector_database_hnsw.cpp
src/lib/vector_database.h        (new unified)
src/lib/vector_database.cpp
```

**Factory Update** (src/lib/lynx.cpp):
```cpp
// Before:
std::shared_ptr<IVectorDatabase> IVectorDatabase::create(const Config& config) {
    if (config.index_type == IndexType::Flat) {
        return std::make_shared<VectorDatabase_Impl>(config);
    } else if (config.index_type == IndexType::HNSW) {
        return std::make_shared<VectorDatabase_MPS>(config);
    } else if (config.index_type == IndexType::IVF) {
        return std::make_shared<VectorDatabase_IVF>(config);
    }
}

// After:
std::shared_ptr<IVectorDatabase> IVectorDatabase::create(const Config& config) {
    return std::make_shared<VectorDatabase>(config);
}
```

**Verification**:
- Full test suite passes
- Examples compile and run
- Documentation updated

## Related Tickets

- Parent: #2051 (Database Architecture Analysis)
- Blocked by: #2060 (Documentation and migration guide)
- Blocks: #2062 (Final validation)
- Phase: Phase 4 - Cleanup
