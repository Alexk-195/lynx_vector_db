# Lynx Vector Database - Implementation State

**Last Updated**: 2025-12-08

## Overall Progress
Phase 1-5 ✅ Complete

## MaintenanceWorker Implementation Ideas

The MaintenanceWorker class (src/lib/mps_workers.h:292) handles background operations to optimize index quality, memory usage, and overall database health. Below are comprehensive ideas for implementing the pending maintenance operations.

### 1. Index Optimization (process_optimize)

#### 1.1 Graph Quality Improvement
- **Edge Pruning**: Remove redundant edges in HNSW graph using RNG (Random Neighbor Graph) heuristic
  - Identify edges that don't contribute to graph connectivity
  - Apply distance-based pruning to maintain sparse yet robust connectivity
  - Preserve graph navigability while reducing memory footprint

- **Graph Rebalancing**: Fix connectivity issues after many insertions/deletions
  - Detect nodes with too few connections (below M/2 threshold)
  - Reconnect isolated or poorly connected nodes
  - Rebuild local neighborhoods for nodes with degraded connectivity

- **Layer Distribution Optimization**: Ensure proper hierarchical structure
  - Verify exponential decay of nodes across layers (controlled by ml parameter)
  - Rebalance if too many nodes concentrate in certain layers
  - Maintain logarithmic search complexity by ensuring proper layer distribution

#### 1.2 Incremental Optimization Strategies
- **Trigger-Based Optimization**: Automatically initiate optimization based on metrics
  - After N insertions (e.g., optimize every 10,000 inserts)
  - When deletion ratio exceeds threshold (e.g., 20% deleted vectors)
  - When average search latency degrades beyond baseline (e.g., +30%)
  - When memory fragmentation exceeds acceptable levels

- **Partial Graph Rebuilding**: Rebuild heavily modified subgraphs
  - Identify "hot zones" with frequent updates
  - Rebuild local neighborhoods without full index reconstruction
  - Use versioning to track graph regions needing attention

#### 1.3 Quality Metrics Collection
- **Graph Connectivity Metrics**:
  - Average path length between nodes
  - Distribution of connections per node (should be close to M)
  - Number of disconnected or isolated nodes
  - Layer occupancy distribution

- **Search Performance Metrics**:
  - Average number of distance calculations per query
  - p50, p95, p99 query latencies
  - Recall degradation over time
  - Cache hit rates for recently accessed nodes

### 2. Storage Compaction (process_compact)

#### 2.1 Memory Reclamation
- **Remove Deleted Vectors**: Clean up soft-deleted entries
  - Scan index for vectors marked as deleted but still occupying memory
  - Rebuild node ID mappings after compaction
  - Update entry point if it references deleted nodes

- **Defragmentation**: Consolidate fragmented memory allocations
  - Identify fragmented regions in vector storage
  - Relocate vectors to contiguous memory blocks
  - Update all internal pointers and references
  - Particularly important for memory-mapped storage

#### 2.2 Compaction Triggers
- **Deletion Ratio Threshold**: Trigger when deleted/total ratio exceeds threshold (e.g., 15%)
- **Memory Waste Threshold**: Trigger when fragmentation wastes more than X MB
- **Periodic Schedule**: Run compaction during low-traffic periods (e.g., daily at 2 AM)
- **Manual Trigger**: Allow explicit compaction via API call

#### 2.3 Compaction Strategies
- **Incremental Compaction**: Compact one segment at a time to avoid long pauses
- **Copy-on-Write**: Create new compacted structure, then atomically swap
- **Background Compaction**: Run at low priority to minimize impact on queries

### 3. Background Maintenance Tasks

#### 3.1 Periodic Health Checks
- **Graph Integrity Verification**:
  - Verify bidirectional edge consistency
  - Check for dangling references to deleted nodes
  - Validate layer structure (every node at layer L must exist at layer L-1)
  - Ensure entry point is valid and optimal

- **Data Consistency Checks**:
  - Verify vector storage matches index entries
  - Check dimension consistency across all vectors
  - Validate metadata integrity
  - Detect and report corruption

#### 3.2 Automatic Persistence
- **Checkpoint Management**:
  - Periodic snapshots to disk (configurable interval)
  - Incremental checkpoints for large databases
  - Keep multiple checkpoint versions for rollback
  - Verify checkpoint integrity before cleanup

- **Write-Ahead Log (WAL) Management** (when implemented):
  - Flush WAL buffers periodically
  - Truncate WAL after successful checkpoint
  - Replay WAL on recovery
  - Monitor WAL size and trigger compaction if growing too large

#### 3.3 Statistics and Monitoring
- **Real-time Statistics Update**:
  - Track insert/delete/query rates
  - Monitor memory usage trends
  - Calculate average query latencies
  - Track recall metrics if ground truth available

- **Performance Degradation Detection**:
  - Compare current metrics to historical baseline
  - Alert when performance degrades beyond threshold
  - Recommend optimization actions based on metrics
  - Log statistics for external monitoring systems

### 4. Advanced Maintenance Features

#### 4.1 Adaptive Parameter Tuning
- **Dynamic ef_search Adjustment**: Adjust based on query patterns
  - Increase ef_search if recall drops below target
  - Decrease ef_search during high load to reduce latency
  - Per-query adaptive tuning based on result quality

- **M Parameter Optimization**: Adjust connection count based on dataset characteristics
  - Higher M for datasets with complex topology
  - Lower M for memory-constrained environments

#### 4.2 Repair and Recovery
- **Automatic Repair**: Fix detected issues without manual intervention
  - Repair broken edges in graph
  - Reconnect isolated nodes
  - Rebuild corrupted layers

- **Graceful Degradation**: Maintain functionality even with partial corruption
  - Isolate corrupted subgraphs
  - Route queries around damaged regions
  - Schedule repair during maintenance window

#### 4.3 Resource Management
- **Memory Budget Enforcement**: Keep memory usage within configured limits
  - Trigger compaction when approaching memory limit
  - Implement cache eviction for query results
  - Use memory-mapped files for cold data

- **CPU Throttling**: Adjust background task intensity based on system load
  - Pause maintenance during high query load
  - Use nice/ionice for background operations
  - Adaptive batch sizing for bulk operations

### 5. Implementation Priorities

**High Priority**:
1. ✅ Basic graph edge pruning in process_optimize() - **COMPLETED**
   - Implemented `HNSWIndex::optimize_graph()` method
   - Applies RNG heuristic to remove redundant edges
   - Maintains graph navigability and search quality
   - Added comprehensive test coverage
2. Deleted vector removal in process_compact()
3. Periodic checkpoint automation

**Medium Priority**:
4. Graph connectivity metrics collection
5. Incremental compaction strategy
6. Graph integrity verification

**Low Priority** (future enhancements):
7. Adaptive parameter tuning
8. Advanced repair mechanisms
9. WAL management (after WAL implementation)

### 6. Design Considerations

- **Non-blocking Operations**: Maintenance should not block queries
  - Use read-write locks appropriately
  - Copy-on-write for structural changes
  - Rate-limit maintenance operations

- **Configurability**: All thresholds and triggers should be configurable
  - Expose via Config structure
  - Allow runtime adjustment where possible
  - Provide sensible defaults

- **Observability**: Emit metrics and logs for monitoring
  - Log maintenance operations
  - Track time spent in each maintenance task
  - Expose metrics via stats() API

- **Safety**: Ensure maintenance cannot corrupt the index
  - Validate preconditions before operations
  - Implement rollback mechanisms
  - Test thoroughly with concurrent operations


