# Ticket: Implement IVF (Inverted File Index) for Lynx Vector Database

*Important*: Read README.md and tickets/README.md if not yet done.

**Priority**: High
**Created**: 2025-12-12
**Assigned**: Unassigned

## Description

Implement the Inverted File Index (IVF) algorithm as an alternative indexing method for Lynx Vector Database. IVF is a cluster-based ANN search method that partitions the dataset into distinct clusters using k-means, reducing search space by limiting exhaustive search to only relevant clusters.

If implementing intra cluster search use index inteface IVectorIndex provided in lynx_intern.h. So that Flat or HNSW Index can be selected for it.

According to CONCEPT.md, IVF should provide:
- Query complexity: O(N/k · n_probe)
- Construction complexity: O(k·D) for k-means
- Memory usage: Low to moderate
- Best suited for: Memory-constrained environments, filtered search

The implementation should follow the existing architecture pattern established by HNSWIndex, implementing the IVectorIndex interface defined in the codebase.

## Acceptance Criteria

- [ ] Create IVFIndex class implementing IVectorIndex interface
- [ ] Implement k-means clustering algorithm for index construction
  - [ ] Support configurable number of clusters (k parameter)
  - [ ] Use existing distance metric functions from utils.h/utils.cpp
- [ ] Implement add() method for inserting vectors into appropriate clusters
- [ ] Implement search() method with configurable n_probe parameter
  - [ ] Find nearest cluster centroids to query vector
  - [ ] Search within selected clusters only
- [ ] Implement remove() method for deleting vectors from clusters
- [ ] Implement build() method for batch construction from vector set
- [ ] Implement serialize() and deserialize() methods for persistence
- [ ] Add IVF configuration parameters to Config struct
  - [ ] ivf_num_clusters (k): number of clusters (default: sqrt(N))
  - [ ] ivf_n_probe: number of clusters to search (default: 8)
  - [ ] ivf_training_vectors: subset size for k-means training
- [ ] Update IndexType enum to include IVF option
- [ ] Create comprehensive unit tests in tests/ivf_index_test.cpp
  - [ ] Test clustering quality
  - [ ] Test insert/search/remove operations
  - [ ] Test recall vs n_probe tradeoffs
  - [ ] Test serialization/deserialization
  - [ ] Test with different distance metrics (L2, Cosine, Dot Product)
- [ ] Add performance benchmarks comparing IVF vs HNSW
- [ ] Update CONCEPT.md with IVF implementation details
- [ ] Update README.md to document IVF as an available index type

## Notes

### Implementation Guidelines

1. **File Organization** (per CLAUDE.md conventions):
   - Header: `src/include/lynx/ivf_index.h`
   - Implementation: `src/lib/ivf_index.cpp`
   - Tests: `tests/ivf_index_test.cpp`

2. **Code Style** (per CLAUDE.md):
   - Use modern C++20 features (std::span, concepts, constexpr)
   - Class name: `IVFIndex` (PascalCase)
   - Methods: `snake_case` (add_vector, search_clusters)
   - Member variables: `snake_case_` with trailing underscore (num_clusters_, centroids_)
   - Constants: `kPascalCase` (kDefaultNumClusters, kDefaultNProbe)

3. **Distance Metrics**:
   - Reuse existing distance calculation functions from `utils.h/utils.cpp`
   - Support all three metrics: L2, Cosine, Dot Product
   - Use utils::calculate_distance() for consistency

4. **K-Means Clustering**:
   - Implement k-means++ initialization for better centroid placement
   - Support configurable max iterations and convergence threshold
   - Consider training on subset of data for large datasets (sampling)
   - Handle edge cases: k > N, empty clusters, single-vector clusters

5. **Thread Safety**:
   - Ensure thread-safe concurrent reads for search operations
   - Serialize writes (add/remove) through MPS IndexWorker
   - Consider using mps::synchronized<T> for cluster data structures

6. **Memory Optimization**:
   - Store vectors efficiently (consider vector of vectors per cluster)
   - Minimize centroid storage overhead
   - Consider memory-mapped storage for large datasets

7. **Performance Considerations**:
   - IVF trades indexing speed for search flexibility
   - Faster index construction than HNSW (no complex graph building)
   - Search speed depends on n_probe vs recall tradeoff
   - Memory usage lower than HNSW (no graph edges)

### Research References

From `doc/research.md`:
- IVF reduces search space by partitioning dataset into clusters using k-means
- Query identifies nearest cluster centroids first
- Exhaustive search limited to vectors in selected clusters only
- Often combined with quantization (IVF-PQ) for memory savings
- Suitable for datasets that naturally segment well

### Testing Strategy

1. **Unit Tests**:
   - Small datasets (100-1000 vectors)
   - Test all public interface methods
   - Test edge cases (empty index, single cluster, k=N)
   - Test with different dimensions (8, 64, 128, 512)

2. **Integration Tests**:
   - Medium datasets (10K-100K vectors)
   - Compare recall vs HNSW and FlatIndex
   - Test persistence (save/load)
   - Test with VectorDatabase class

3. **Benchmarks**:
   - Compare indexing time vs HNSW
   - Compare query latency vs recall curves
   - Compare memory usage
   - Test scalability (1M+ vectors)

### Future Enhancements

Consider for follow-up tickets:
- IVF-PQ: Combine with Product Quantization for memory savings (ticket 2001)
- IVF-HNSW: Use HNSW for intra-cluster search instead of flat search (ticket 2002)
- GPU-accelerated k-means for faster index construction (ticket 2003)
- Dynamic cluster rebalancing as dataset grows (ticket 2004)

## Related Tickets

- Related: HNSW implementation (completed)
- Blocks: #2001 (IVF-PQ implementation)
- Blocks: #2002 (IVF-HNSW hybrid)
