# Ticket: Implement IVF Remove, Build, and Persistence

*Important*: Read README.md and tickets/README.md if not yet done.

**Priority**: High
**Created**: 2025-12-12
**Assigned**: Claude
**Status**: Complete
**Completed**: 2025-12-12

## Description

Complete the IVFIndex implementation by adding:
1. **remove()** method to delete vectors from the index
2. **build()** method for efficient batch construction from a dataset
3. **serialize()** and **deserialize()** methods for index persistence

These methods complete the IVectorIndex interface implementation and enable full functionality of the IVF index.

## Acceptance Criteria

- [x] Implement `remove()` method
  - [x] Locate vector's cluster using ID-to-cluster mapping
  - [x] Remove from inverted list (ID and vector data)
  - [x] Update ID-to-cluster mapping
  - [x] Handle non-existent IDs gracefully
- [x] Implement `build()` method for batch construction
  - [x] Run k-means clustering on input vectors to create centroids
  - [x] Support training on subset for large datasets (sampling)
  - [x] Assign all vectors to appropriate clusters
  - [x] Build inverted lists efficiently
  - [x] Validate that build() creates proper index structure
- [x] Implement `serialize()` method
  - [x] Save index metadata (dimension, metric, params)
  - [x] Save centroids
  - [x] Save inverted lists (structure-preserving format)
  - [x] Save ID-to-cluster mapping
- [x] Implement `deserialize()` method
  - [x] Load and validate index metadata
  - [x] Reconstruct centroids
  - [x] Reconstruct inverted lists
  - [x] Reconstruct ID-to-cluster mapping
  - [x] Validate index integrity after loading
- [x] Add comprehensive unit tests in `tests/test_ivf_index.cpp`
  - [x] Test remove() operation
  - [x] Test build() with various dataset sizes
  - [x] Test serialize/deserialize round-trip
  - [x] Test persistence preserves search results
- [x] Add integration test for save/load workflow

## Notes

### Implementation Guidelines

1. **remove() Implementation**:
   ```cpp
   ErrorCode IVFIndex::remove(std::uint64_t id) {
       std::unique_lock lock(mutex_);

       // Find which cluster contains this ID
       auto it = id_to_cluster_.find(id);
       if (it == id_to_cluster_.end()) {
           return ErrorCode::VectorNotFound;
       }

       std::size_t cluster_id = it->second;
       auto& inv_list = inverted_lists_[cluster_id];

       // Find position in inverted list
       auto id_it = std::find(inv_list.ids.begin(), inv_list.ids.end(), id);
       if (id_it == inv_list.ids.end()) {
           return ErrorCode::InvalidState;  // Inconsistent state
       }

       std::size_t pos = std::distance(inv_list.ids.begin(), id_it);

       // Remove from inverted list (swap with last for O(1) removal)
       if (pos != inv_list.ids.size() - 1) {
           std::swap(inv_list.ids[pos], inv_list.ids.back());
           std::swap(inv_list.vectors[pos], inv_list.vectors.back());
       }
       inv_list.ids.pop_back();
       inv_list.vectors.pop_back();

       // Remove from mapping
       id_to_cluster_.erase(it);

       return ErrorCode::Ok;
   }
   ```

2. **build() Implementation**:
   ```cpp
   ErrorCode IVFIndex::build(std::span<const VectorRecord> vectors) {
       if (vectors.empty()) {
           return ErrorCode::InvalidParameter;
       }

       std::unique_lock lock(mutex_);

       // Clear existing data
       inverted_lists_.clear();
       centroids_.clear();
       id_to_cluster_.clear();

       // Extract vector data for k-means
       std::vector<std::vector<float>> vec_data;
       vec_data.reserve(vectors.size());
       for (const auto& rec : vectors) {
           vec_data.push_back(rec.vector);
       }

       // Run k-means clustering
       clustering::KMeans kmeans(params_.n_clusters, dimension_, metric_, {});
       kmeans.fit(vec_data);
       centroids_ = kmeans.centroids();

       // Initialize inverted lists
       inverted_lists_.resize(centroids_.size());

       // Assign vectors to clusters
       auto assignments = kmeans.predict(vec_data);
       for (std::size_t i = 0; i < vectors.size(); ++i) {
           std::size_t cluster_id = assignments[i];
           inverted_lists_[cluster_id].ids.push_back(vectors[i].id);
           inverted_lists_[cluster_id].vectors.push_back(vectors[i].vector);
           id_to_cluster_[vectors[i].id] = cluster_id;
       }

       return ErrorCode::Ok;
   }
   ```

3. **Serialization Format**:
   ```
   Header:
   - Magic number (4 bytes): "IVFX"
   - Version (4 bytes): 1
   - Dimension (8 bytes)
   - Distance metric (4 bytes)
   - Num clusters (8 bytes)
   - Total vectors (8 bytes)

   Centroids:
   - For each centroid: dimension * 4 bytes (floats)

   Inverted Lists:
   - For each cluster:
     - List size (8 bytes)
     - Vector IDs (list_size * 8 bytes)
     - Vector data (list_size * dimension * 4 bytes)

   ID Mapping:
   - Num entries (8 bytes)
   - For each entry: id (8 bytes), cluster_id (8 bytes)
   ```

4. **serialize() Implementation**:
   ```cpp
   ErrorCode IVFIndex::serialize(std::ostream& out) const {
       std::shared_lock lock(mutex_);

       // Write header
       out.write("IVFX", 4);
       uint32_t version = 1;
       out.write(reinterpret_cast<const char*>(&version), sizeof(version));
       out.write(reinterpret_cast<const char*>(&dimension_), sizeof(dimension_));
       uint32_t metric = static_cast<uint32_t>(metric_);
       out.write(reinterpret_cast<const char*>(&metric), sizeof(metric));

       // Write centroids
       std::size_t num_clusters = centroids_.size();
       out.write(reinterpret_cast<const char*>(&num_clusters), sizeof(num_clusters));
       for (const auto& centroid : centroids_) {
           out.write(reinterpret_cast<const char*>(centroid.data()),
                    dimension_ * sizeof(float));
       }

       // Write inverted lists
       for (const auto& inv_list : inverted_lists_) {
           std::size_t list_size = inv_list.ids.size();
           out.write(reinterpret_cast<const char*>(&list_size), sizeof(list_size));
           out.write(reinterpret_cast<const char*>(inv_list.ids.data()),
                    list_size * sizeof(std::uint64_t));
           for (const auto& vec : inv_list.vectors) {
               out.write(reinterpret_cast<const char*>(vec.data()),
                        dimension_ * sizeof(float));
           }
       }

       // Write ID mapping
       std::size_t map_size = id_to_cluster_.size();
       out.write(reinterpret_cast<const char*>(&map_size), sizeof(map_size));
       for (const auto& [id, cluster] : id_to_cluster_) {
           out.write(reinterpret_cast<const char*>(&id), sizeof(id));
           out.write(reinterpret_cast<const char*>(&cluster), sizeof(cluster));
       }

       return out.good() ? ErrorCode::Ok : ErrorCode::IOError;
   }
   ```

5. **deserialize() Implementation**:
   - Mirror of serialize() but with validation
   - Check magic number and version
   - Validate dimension matches constructor
   - Verify data integrity (e.g., all referenced clusters exist)

### Testing Strategy

1. **remove() Tests**:
   - Build index, remove vector, verify it's gone
   - Test remove non-existent ID
   - Test remove from different clusters
   - Verify search results change after removal

2. **build() Tests**:
   - Build with small dataset (100 vectors)
   - Build with medium dataset (10K vectors)
   - Verify clustering quality
   - Test with different num_clusters values

3. **Serialization Tests**:
   - Serialize empty index
   - Serialize index with data
   - Deserialize and verify exact match
   - Verify search results identical before/after save/load
   - Test error handling (corrupted file, wrong version)

4. **Integration Tests**:
   - Full workflow: build -> add vectors -> search -> save -> load -> search
   - Verify results consistent across save/load boundary

## Related Tickets

- Parent: #2000 (Implement IVF)
- Blocked by: #2001 (K-Means), #2002 (IVFIndex structure), #2003 (IVF search)
- Blocks: #2005 (Testing and benchmarks)
