![Build Status](https://github.com/Alexk-195/lynx_vector_db/actions/workflows/c-cpp.yml/badge.svg)

# Lynx Vector Database

A fast and light weight vector database implemented in modern C++20 with support for Approximate Nearest Neighbor (ANN) search algorithms. Suitable for medium size projects on embedded systems. 

## Features

- **Multiple Index Types**:
  - **HNSW**: Hierarchical Navigable Small World graphs for O(log N) query time and high recall
  - **IVF**: Inverted File Index for fast construction and memory-efficient approximate search
  - **Flat**: Brute-force exact search for small datasets
- **Multi-threaded**: Built on MPS (Message Processing System) for concurrent operations
- **Modern C++20**: Concepts, spans, ranges, and coroutines
- **Flexible Distance Metrics**: L2 (Euclidean), Cosine, Dot Product
- **Persistence**: Save and load indices to/from disk

## Requirements

- C++20 compatible compiler (GCC 11+, Clang 14+)
- GNU Make or CMake 3.20+
- Git (for automatic dependency cloning)

### Dependencies (Auto-managed)

The following dependencies are **automatically handled** by the build scripts:

- **MPS library** ([github.com/Alexk-195/mps](https://github.com/Alexk-195/mps))
  - Checked in order: `MPS_PATH` env var → `MPS_DIR` env var → `external/mps` → auto-clone
- **Google Test v1.15.2** ([github.com/google/googletest](https://github.com/google/googletest))
  - Automatically cloned to `external/googletest` or fetched by CMake

## Quick Start

### Clone and Build

```bash
git clone https://github.com/Alexk-195/lynx_vector_db.git
cd lynx_vector_db

# Build with setup.sh (recommended - handles all dependencies automatically)
./setup.sh

# OR build with CMake (dependencies auto-detected or fetched)
mkdir build && cd build
cmake ..
cmake --build .
```

### Custom Dependency Locations

If you have MPS installed elsewhere, set the environment variable:

```bash
# Option 1: Use MPS_PATH
export MPS_PATH=/path/to/mps
./setup.sh

# Option 2: Use MPS_DIR (legacy)
export MPS_DIR=/path/to/mps
./setup.sh

# Option 3: Let setup.sh auto-clone to external/mps (no env var needed)
./setup.sh
```

### Build Options - setup.sh (Makefile)

```bash
./setup.sh          # Build release version
./setup.sh debug    # Build debug version
./setup.sh clean    # Clean build artifacts
./setup.sh rebuild  # Clean and rebuild
./setup.sh test     # Run unit tests
./setup.sh install  # Install to system (requires sudo)
./setup.sh coverage  # Do test coverage
```

### Build Options - Make

```bash
make              # Build release
make debug        # Build debug
make test         # Build and run tests
make build-tests  # Build tests only
make clean        # Clean build
make run          # Build and run
make info         # Show build configuration

# Note: MPS_DIR is automatically set by setup.sh
# If using make directly, set MPS_PATH or MPS_DIR manually
export MPS_PATH=/path/to/mps
make
```

### Build Options - CMake

```bash
# Configure (dependencies auto-detected)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# With custom MPS location (if needed)
cmake .. -DMPS_PATH=/path/to/mps
# OR
cmake .. -DMPS_DIR=/path/to/mps

# Build
cmake --build . --parallel

# Run tests
ctest              # Individual test discovery
make test          # CMake test target
make check         # Verbose test output

# Install
sudo cmake --install .
```

## Usage

### Quick Example - HNSW Index

```cpp
#include <lynx/lynx.h>

int main() {
    // Create database with HNSW index
    lynx::Config config;
    config.dimension = 128;
    config.index_type = lynx::IndexType::HNSW;
    config.hnsw_params.M = 16;
    config.hnsw_params.ef_construction = 200;

    auto db = lynx::IVectorDatabase::create(config);

    // Insert vectors
    std::vector<float> vec1(128, 0.5f);
    db->insert({1, vec1, std::nullopt});

    // Search
    std::vector<float> query(128, 0.5f);
    auto results = db->search(query, 10);

    return 0;
}
```

### IVF Index Example

```cpp
#include <lynx/lynx.h>

int main() {
    // Create database with IVF index
    lynx::Config config;
    config.dimension = 128;
    config.index_type = lynx::IndexType::IVF;
    config.ivf_params.n_clusters = 100;     // Number of clusters
    config.ivf_params.n_probe = 10;         // Clusters to search

    auto db = lynx::IVectorDatabase::create(config);

    // Build index from batch data
    std::vector<lynx::VectorRecord> records;
    for (size_t i = 0; i < 10000; ++i) {
        std::vector<float> vec(128);
        // ... fill vector ...
        records.push_back({i, vec, std::nullopt});
    }
    db->batch_insert(records);

    // Search with custom n_probe
    std::vector<float> query(128, 0.5f);
    lynx::SearchParams params;
    params.n_probe = 20;  // Search more clusters for higher recall
    auto results = db->search(query, 10, params);

    return 0;
}
```

For more examples see [src/main_minimal.cpp](src/main_minimal.cpp) and [src/main.cpp](src/main.cpp)

### Choosing an Index Type

| Index Type | Best For | Query Speed | Memory | Construction | Recall |
|------------|----------|-------------|---------|--------------|---------|
| **Flat** | Small datasets (<1K), exact search required | Slow (O(N)) | Low | Instant | 100% |
| **HNSW** | High-performance ANN, low latency critical | Fast (O(log N)) | High | Slow | 95-99% |
| **IVF** | Memory-constrained, fast construction needed | Medium | Low | Fast | 85-98% |

### IVF Parameter Tuning

**n_clusters** (number of clusters):
- Rule of thumb: `sqrt(N)` where N is the number of vectors
- Example: For 10,000 vectors, use ~100 clusters
- Larger values: Faster search but may require higher n_probe for good recall
- Smaller values: Slower search but better recall with low n_probe

**n_probe** (clusters to search):
- Controls the speed/recall tradeoff
- Start with 10 and adjust based on your needs:
  - n_probe=1: Fastest queries, ~60-70% recall
  - n_probe=10: Balanced, ~90-95% recall
  - n_probe=sqrt(n_clusters): High recall, ~98%+
- Higher values = better recall but slower queries

**Example tuning process**:
```cpp
// Start with default configuration
config.ivf_params.n_clusters = 100;  // For ~10K vectors
config.ivf_params.n_probe = 10;

// If recall is too low, increase n_probe
params.n_probe = 20;  // Better recall, slower

// If queries are too slow, decrease n_probe
params.n_probe = 5;   // Faster, lower recall

// For datasets with natural clusters, fewer clusters may work better
config.ivf_params.n_clusters = 50;  // Try fewer clusters
```

## Project Structure

```
lynx_vector_db/
├── Makefile             # Build configuration
├── setup.sh             # Build script
├── CLAUDE.md            # AI assistant instructions
├── CONCEPT.md           # Architecture and design
├── README.md            # This file
├── LICENSE              # MIT License
├── doc/
│   └── research.md      # ANN algorithm research and other files
├── tickets/
│   ├── README.md        # Ticketing system documentation
│   └── done/            # Completed tickets
├── src/
│   ├── include/
│   │   └── lynx/
│   │       └── lynx.h   # Public interface
│   ├── lib/
│   │   └── ...          # Implementation files
│   ├── main_minimal.cpp         # Example of minimal database executable
│   └── main.cpp         # Larger example of database usage
└── tests/
    └── ...              # Unit tests
```

## Architecture

Lynx uses a layered architecture:

1. **API Layer**: Pure virtual interface (`IVectorDatabase`)
2. **Index Layer**: HNSW implementations
3. **Threading Layer**: MPS pools and workers
4. **Storage Layer**: Memory-mapped persistence

See [CONCEPT.md](CONCEPT.md) for detailed design documentation.

## Documentation

- [CONCEPT.md](CONCEPT.md) - Design concept
- [doc/STATE.md](STATE.md) - Current implementation state
- [doc/research.md](doc/research.md) - ANN algorithm research
- [tests/README.md](tests/README.md) - Infos about unit testing
- [tickets/README.md](tickets/README.md) - File-based ticketing system

## Contributing

1. Read [CLAUDE.md](CLAUDE.md) for coding guidelines
2. Review open tickets in [tickets/](tickets/) for tasks to work on
3. Follow the code style conventions
4. Add tests for new functionality

## License

MIT License - see [LICENSE](LICENSE) for details.

