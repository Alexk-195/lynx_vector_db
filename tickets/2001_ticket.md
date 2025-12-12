# Ticket: Implement K-Means Clustering Algorithm

*Important*: Read README.md and tickets/README.md if not yet done.

**Priority**: High
**Created**: 2025-12-12
**Assigned**: Unassigned

## Description

Implement a k-means clustering algorithm as a foundational component for the IVF (Inverted File Index) implementation. This algorithm will partition vectors into k distinct clusters by iteratively assigning vectors to nearest centroids and updating centroids based on cluster membership.

The k-means implementation will be used by IVFIndex to create the cluster structure during index construction.

## Acceptance Criteria

- [ ] Create KMeans class in `src/lib/kmeans.h` and `src/lib/kmeans.cpp`
- [ ] Implement k-means++ initialization for better initial centroid placement
  - [ ] Choose first centroid uniformly at random
  - [ ] Choose subsequent centroids with probability proportional to D(x)^2
- [ ] Implement Lloyd's algorithm for clustering
  - [ ] Assignment step: assign each vector to nearest centroid
  - [ ] Update step: recompute centroids as mean of assigned vectors
  - [ ] Convergence detection (centroid movement threshold or max iterations)
- [ ] Support all three distance metrics (L2, Cosine, DotProduct) from utils.h
- [ ] Handle edge cases:
  - [ ] k > N (more clusters than vectors)
  - [ ] Empty clusters during iterations
  - [ ] Single-vector clusters
- [ ] Configurable parameters:
  - [ ] max_iterations (default: 100)
  - [ ] convergence_threshold (default: 1e-4)
  - [ ] random_seed for reproducibility
- [ ] Support training on subset of data for large datasets (sampling)
- [ ] Add unit tests in `tests/test_kmeans.cpp`
  - [ ] Test convergence with known data
  - [ ] Test with different metrics
  - [ ] Test edge cases
  - [ ] Test k-means++ initialization quality

## Notes

### Implementation Guidelines

1. **File Structure**:
   ```cpp
   // kmeans.h
   namespace lynx {
   namespace clustering {

   struct KMeansParams {
       std::size_t max_iterations = 100;
       float convergence_threshold = 1e-4f;
       std::optional<std::uint64_t> random_seed = std::nullopt;
   };

   class KMeans {
   public:
       KMeans(std::size_t k, std::size_t dimension,
              DistanceMetric metric, const KMeansParams& params);

       // Fit k-means on vectors
       void fit(std::span<const std::vector<float>> vectors);

       // Get cluster assignments for vectors
       std::vector<std::size_t> predict(std::span<const std::vector<float>> vectors) const;

       // Get centroids
       const std::vector<std::vector<float>>& centroids() const;

   private:
       std::size_t k_;
       std::size_t dimension_;
       DistanceMetric metric_;
       KMeansParams params_;
       std::vector<std::vector<float>> centroids_;

       void initialize_centroids_plusplus(std::span<const std::vector<float>> vectors);
       std::size_t assign_to_nearest_centroid(std::span<const float> vector) const;
       void update_centroids(std::span<const std::vector<float>> vectors,
                           const std::vector<std::size_t>& assignments);
   };

   } // namespace clustering
   } // namespace lynx
   ```

2. **Algorithm Steps**:
   - K-means++ initialization ensures good initial centroids
   - Lloyd's algorithm alternates between assignment and update steps
   - Convergence when centroid movement < threshold or max iterations reached

3. **Edge Case Handling**:
   - If k > N: reduce k to N and issue warning
   - Empty clusters: reinitialize empty centroids to random vectors
   - Use stable numerics for centroid updates

4. **Distance Metrics**:
   - Reuse `utils::calculate_distance()` for consistency
   - For cosine similarity, normalize vectors before clustering

5. **Performance Considerations**:
   - Cache distance calculations where possible
   - Consider parallel assignment step for large datasets
   - Early termination when centroids stabilize

### Testing Strategy

1. **Synthetic Data**:
   - Test with 3 well-separated Gaussian clusters (100 vectors each)
   - Verify correct convergence and cluster assignments

2. **Edge Cases**:
   - Test k=1 (single cluster)
   - Test k=N (each vector is its own cluster)
   - Test with duplicate vectors

3. **Metrics**:
   - Verify clustering quality with different metrics
   - Compare k-means++ vs random initialization

4. **Reproducibility**:
   - Test that same random_seed produces same results

## Related Tickets

- Parent: #2000 (Implement IVF)
- Blocks: #2002 (IVFIndex class structure)
