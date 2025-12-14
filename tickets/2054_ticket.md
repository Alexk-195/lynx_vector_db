# Ticket 2054: Design Unified VectorDatabase

**Priority**: High
**Created**: 2025-12-14
**Assigned**: Unassigned

## Description

Design a unified `VectorDatabase` class that works with all index types (Flat, HNSW, IVF) through the `IVectorIndex` interface. This design eliminates code duplication across three current database implementations and establishes a clean separation between database management and index algorithms.

## Acceptance Criteria

- [ ] Design document covering:
  - [ ] Class structure and responsibilities
  - [ ] Common operations (insert, search, remove, batch_insert)
  - [ ] Vector storage strategy
  - [ ] Statistics tracking
  - [ ] Persistence (save/load)
  - [ ] Threading abstraction design
  - [ ] Index delegation pattern
- [ ] Define threading strategy:
  - [ ] Simple mode: `std::shared_mutex` for concurrent reads/writes
  - [ ] No complex MPS infrastructure in core database
- [ ] Interface definition:
  - [ ] Constructor parameters
  - [ ] Public methods
  - [ ] Private data members
- [ ] Error handling strategy
- [ ] Migration path from existing implementations
- [ ] Design review and approval

## Notes

**Key Design Principles**:
1. Single database implementation for all index types
2. Delegation pattern: database → index operations
3. Common vector storage: `std::unordered_map<uint64_t, VectorRecord>`
4. Simple threading: `std::shared_mutex` (readers/writers)
5. Statistics and persistence at database level
6. Index-specific operations delegated to `IVectorIndex*`

**Reference Architecture** (from ticket 2051):
```
VectorDatabase (single implementation)
├── Common Operations: vector storage, statistics, persistence
├── Thread safety: std::shared_mutex
└── Delegates to: IVectorIndex* (polymorphism)
    ├── FlatIndex
    ├── HNSWIndex
    └── IVFIndex
```

**Threading Design**:
- Use `std::shared_mutex` for vector storage
- Multiple concurrent readers (search, get, contains)
- Exclusive writer access (insert, remove, batch_insert)
- Simple and maintainable

## Related Tickets

- Parent: #2051 (Database Architecture Analysis)
- Blocked by: #2053 (Test FlatIndex integration)
- Blocks: #2055 (Implement core VectorDatabase)
- Phase: Phase 2 - Create Unified VectorDatabase
