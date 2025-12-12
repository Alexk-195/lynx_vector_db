# Ticket 1010 Result: Insertion timings for indices

**Completed**: 2025-12-12
**Resolved by**: Claude Code

## Summary

Extended compare_indices.cpp to measure and display insertion times separately for both Flat and HNSW index types. The implementation now shows timing comparison between the two index types during vector insertion, helping understand the performance trade-offs.

## Changes Made

- Added `<chrono>` include for high-resolution timing measurements
- Pre-generate all vectors before insertion to ensure fair timing comparison
- Separate insertion loops with timing for Flat index (lines 183-193)
- Separate insertion loops with timing for HNSW index (lines 195-205)
- Added insertion timing output section displaying:
  - Flat index insertion time in milliseconds
  - HNSW index insertion time in milliseconds
  - Ratio showing how much slower HNSW is compared to Flat

## Commits

- a418afd Add insertion timing measurements to compare_indices

## Testing

Executed `./build/bin/compare_indices` successfully. Output shows:
- Flat index: 1 ms for 1000 vectors
- HNSW index: 1531 ms for 1000 vectors
- Ratio: 1.5e+03x (HNSW is ~1500x slower due to graph construction overhead)

## Notes

The HNSW index is significantly slower during insertion because it needs to build and maintain the hierarchical graph structure, while the Flat index simply stores vectors in memory. This trade-off is expected: HNSW sacrifices insertion speed for much faster query performance, as demonstrated in the query results section where HNSW shows substantial speedup over Flat index searches.
