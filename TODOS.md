### TODOS
- [x] Add tests to tests/test_hnsw.cpp to test functions compact_index, serialize and deserialize of class HNSWIndex which is implemented in src/lib/hnsw_index.h and src/lib/hnsw_index.cpp
- [x] Implement WriteLog for non-blocking index maintenance (doc/write_log.md)
  - Added WriteLog struct to capture insert/remove operations during maintenance
  - Implemented clone-optimize-replay-swap pattern in VectorDatabase_MPS::optimize_index()
  - Added ErrorCode::Busy for high-load scenarios
  - Added comprehensive tests in tests/test_write_log.cpp
