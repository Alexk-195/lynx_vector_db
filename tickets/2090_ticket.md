# Ticket: Add profiling build and reporting

*Important*: Read README.md and tickets/README.md if not yet done.

## Description
Insertion times for HNSW and IVF indices are quite large. I want to profile and analyze the code for performance improvements. 
Add profiling possibility to the application "src/compare_indices.cpp" to profile all three index types. 
Generate report where it's visible which functions need improvement.
Take care that profiling only consideres files in src/lib.

## Acceptance Criteria

- [ ] Profile report generated
