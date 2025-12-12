# Ticket: Implement IVF Search with N-Probe

*Important*: Read README.md and tickets/README.md if not yet done.

**Priority**: High
**Created**: 2025-12-12
**Assigned**: Unassigned

## Description

Implement the search() method for IVFIndex with configurable n_probe parameter. The IVF search algorithm works by:
1. Finding the n_probe nearest cluster centroids to the query vector
2. Searching only within those selected clusters (inverted lists)
3. Returning the k nearest neighbors from the combined results

This provides a tunable tradeoff between search speed and recall quality.

## Acceptance Criteria

- [ ] Implement `search()` method in IVFIndex
  - [ ] Find n_probe nearest centroids to query vector
  - [ ] Search within selected clusters only
  - [ ] Aggregate candidates from all probed clusters
  - [ ] Return top-k nearest neighbors sorted by distance
- [ ] Support SearchParams.n_probe parameter
  - [ ] Default to IVFParams.n_probe if not specified
  - [ ] Clamp n_probe to valid range [1, num_clusters]
- [ ] Handle edge cases:
  - [ ] Empty clusters (skip gracefully)
  - [ ] n_probe > num_clusters (search all clusters)
  - [ ] k > total vectors in probed clusters
- [ ] Optimize search performance:
  - [ ] Use priority queue for efficient top-k selection
  - [ ] Early termination when possible
- [ ] Add comprehensive unit tests in `tests/test_ivf_index.cpp`
  - [ ] Test recall vs n_probe tradeoff
  - [ ] Test with different k values
  - [ ] Test with all distance metrics
  - [ ] Test edge cases
- [ ] Add integration tests comparing IVF vs Flat index recall

## Notes

### Implementation Guidelines

1. **Search Algorithm**:
   ```cpp
   std::vector<SearchResultItem> IVFIndex::search(
       std::span<const float> query,
       std::size_t k,
       const SearchParams& params) const {

       // Validate dimension
       if (query.size() != dimension_) {
           return {};  // Empty result on mismatch
       }

       std::shared_lock lock(mutex_);

       // Get n_probe from params or use default
       std::size_t n_probe = params.n_probe.value_or(params_.n_probe);
       n_probe = std::min(n_probe, centroids_.size());

       // Step 1: Find n_probe nearest centroids
       std::vector<std::size_t> probe_clusters = find_nearest_centroids(query, n_probe);

       // Step 2: Search within selected clusters
       std::vector<SearchResultItem> candidates;
       for (std::size_t cluster_id : probe_clusters) {
           const auto& inv_list = inverted_lists_[cluster_id];
           for (std::size_t i = 0; i < inv_list.ids.size(); ++i) {
               float dist = calculate_distance(query, inv_list.vectors[i]);
               candidates.push_back({inv_list.ids[i], dist});
           }
       }

       // Step 3: Select top-k results
       std::partial_sort(candidates.begin(),
                        candidates.begin() + std::min(k, candidates.size()),
                        candidates.end(),
                        [](const auto& a, const auto& b) { return a.distance < b.distance; });

       if (candidates.size() > k) {
           candidates.resize(k);
       }

       return candidates;
   }
   ```

2. **Finding Nearest Centroids**:
   ```cpp
   std::vector<std::size_t> find_nearest_centroids(
       std::span<const float> query,
       std::size_t n_probe) const {

       // Calculate distances to all centroids
       std::vector<std::pair<float, std::size_t>> centroid_distances;
       centroid_distances.reserve(centroids_.size());

       for (std::size_t i = 0; i < centroids_.size(); ++i) {
           float dist = calculate_distance(query, centroids_[i]);
           centroid_distances.push_back({dist, i});
       }

       // Select n_probe nearest
       std::partial_sort(centroid_distances.begin(),
                        centroid_distances.begin() + n_probe,
                        centroid_distances.end());

       std::vector<std::size_t> result;
       result.reserve(n_probe);
       for (std::size_t i = 0; i < n_probe; ++i) {
           result.push_back(centroid_distances[i].second);
       }

       return result;
   }
   ```

3. **Performance Optimizations**:
   - Use partial_sort instead of full sort for top-k selection
   - Consider vectorized distance calculations
   - Skip empty clusters efficiently
   - Cache frequently accessed data

4. **Recall vs Speed Tradeoff**:
   - n_probe=1: Fastest, lowest recall (only search nearest cluster)
   - n_probe=k: Balanced
   - n_probe=num_clusters: Exhaustive search (100% recall, same as Flat)

5. **Alternative: Use Flat or HNSW for Intra-Cluster Search**:
   - The ticket mentions optionally using IVectorIndex for intra-cluster search
   - This could be a future enhancement (not required for this ticket)
   - Current implementation uses brute-force within clusters

### Testing Strategy

1. **Correctness Tests**:
   - Create index with known centroids and vectors
   - Verify correct clusters are probed for given query
   - Verify top-k results are correctly sorted

2. **Recall Tests**:
   - Generate random dataset (10K vectors)
   - Build IVF index with k=100 clusters
   - Compare IVF search results vs brute-force (Flat) search
   - Measure recall@k for different n_probe values:
     - n_probe=1: expect ~60-70% recall
     - n_probe=10: expect ~90-95% recall
     - n_probe=20: expect ~95-98% recall

3. **Edge Cases**:
   - Empty index search
   - Search with k > total vectors
   - Search with n_probe > num_clusters
   - Query vector with wrong dimension

4. **Performance Tests**:
   - Measure query latency vs n_probe
   - Compare against HNSW and Flat indices
   - Verify linear scaling with n_probe

## Related Tickets

- Parent: #2000 (Implement IVF)
- Blocked by: #2002 (IVFIndex class structure)
- Blocks: #2004 (Remove, build, persistence)
