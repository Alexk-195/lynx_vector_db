### TODOS
Make sure you have read other md files in this directory before start implementation.

- [x] Add support for coverage build and run to CMakeLists.txt. Use it from setup.sh script. Remove it from Makefile.
- [x] Review src/lib/vector_database_impl.cpp for correctness. Implement save() and load() in src/lib/vector_database_impl.cpp
- [x] Modify main.cpp so that index type can be provided on command line
- [x] Analyze why results with Flat index (brute force) implementation differ from results with HNSW index. See ANALYSIS.md for detailed findings.
- [ ] Strip down Makefile to functionality of building lib and executables. Remove unit test build. Move compare_indices.cpp to src folder. Add building to CMakeLists. Move Analysis.md to do folder
      

