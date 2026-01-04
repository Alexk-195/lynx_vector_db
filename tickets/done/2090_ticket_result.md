# Ticket 2090 Result: Add profiling build and reporting

**Completed**: 2025-12-18
**Resolved by**: Claude Code Agent

## Summary

Successfully implemented profiling support for Lynx Vector Database to identify performance bottlenecks in HNSW and IVF index insertion operations. Added a new `./setup.sh profile` command that builds the benchmark with profiling instrumentation, runs it, and generates a detailed performance report focusing on src/lib functions.

## Changes Made

1. **Added profiling build option to setup.sh**:
   - New `./setup.sh profile` command builds with `-pg` flag for gprof profiling
   - Automatically compiles all src/lib/*.cpp files with profiling instrumentation
   - Links benchmark with profiled library

2. **Automated profiling workflow**:
   - Builds profiled version of benchmark (benchmarks/lynx_test.cpp)
   - Runs benchmark to generate profiling data (gmon.out)
   - Generates comprehensive profile report using gprof
   - Filters report to highlight src/lib functions
   - Creates two output files:
     - `tickets/2090_profile_report.txt` - Filtered report with key insights
     - `tickets/2090_profile_full.txt` - Complete gprof output

3. **Updated documentation**:
   - Added usage instructions to setup.sh header
   - Updated help text to include profile option

## Key Findings from Profile Report

The profiling revealed critical performance bottlenecks in HNSW index insertion:

### Top Time-Consuming Functions (src/lib)

1. **lynx::utils::calculate_l2_squared** - 90.82% (10.68s)
   - Called 19,205,689 times
   - This is the primary bottleneck
   - Potential optimization: SIMD vectorization, inline assembly

2. **lynx::HNSWIndex::search_layer** - 2.72% (0.32s)
   - Called 2,207 times
   - Graph traversal during insertion

3. **lynx::HNSWIndex::select_neighbors_heuristic** - 2.64% (0.31s)
   - Called 60,869 times
   - Neighbor selection algorithm

4. **lynx::utils::calculate_l2** - 1.11% (0.13s)
   - Called 19,205,689 times
   - Wrapper around calculate_l2_squared

5. **lynx::HNSWIndex::serialize** - 0.94% (0.11s)
   - Called 37,883,576 times
   - Vector data access overhead

### Performance Metrics

- **Total insertion time**: 13.99 seconds for 1,000 vectors (512 dimensions)
- **Distance calculations dominate**: 91.93% of total runtime
- **HNSW graph operations**: ~5.5% of total runtime

### Recommended Optimizations

1. **Critical**: Optimize distance calculation functions
   - Add SIMD vectorization (AVX2/AVX-512)
   - Consider inline assembly for critical path
   - Cache-friendly memory access patterns

2. **High Priority**: Reduce distance calculation frequency
   - Cache frequently computed distances
   - Early termination strategies
   - Better neighbor pruning heuristics

3. **Medium Priority**: Optimize graph operations
   - Improve select_neighbors_heuristic efficiency
   - Better data structures for neighbor lists

## Testing

The profiling workflow was tested with:
- 1,000 vectors, 512 dimensions
- HNSW index (M=32, ef_construction=200)
- L2 distance metric
- Successfully generated detailed profile reports

## Usage

```bash
# Run profiling on benchmark
./setup.sh profile

# View report
cat tickets/2090_profile_report.txt

# View full gprof output
cat tickets/2090_profile_full.txt
```

## Files Modified

- `setup.sh` - Added profile build option

## Files Created

- `tickets/2090_profile_report.txt` - Filtered performance report
- `tickets/2090_profile_full.txt` - Complete gprof output

## Notes

- Profiling uses gprof with -pg compiler flag
- Profiled builds use -O2 optimization to balance profiling accuracy and realistic performance
- The profiling framework can be extended to profile IVF index insertion as well by modifying the benchmark configuration
- Profile reports clearly identify that distance calculation optimization should be the top priority for performance improvements
