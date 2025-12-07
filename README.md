# Lynx Vector Database

A high-performance vector database implemented in modern C++20 with support for Approximate Nearest Neighbor (ANN) search algorithms.

## Features

- **HNSW Index**: Hierarchical Navigable Small World graphs for O(log N) query time
- **IVF Index**: Inverted File Index for memory-efficient search (planned)
- **Multi-threaded**: Built on MPS (Message Processing System) for concurrent operations
- **Modern C++20**: Concepts, spans, ranges, and coroutines
- **Flexible Distance Metrics**: L2 (Euclidean), Cosine, Dot Product

## Requirements

- C++20 compatible compiler (GCC 11+, Clang 14+)
- CMake 3.20+
- MPS library ([github.com/Alexk-195/mps](https://github.com/Alexk-195/mps))

## Quick Start

### Clone and Build

```bash
git clone https://github.com/Alexk-195/lynx_vector_db.git
cd lynx_vector_db

# Clone MPS dependency
git clone https://github.com/Alexk-195/mps.git ../mps
export MPS_DIR=$(pwd)/../mps

# Build
./setup.sh
```

### Build Options

```bash
./setup.sh          # Build release version
./setup.sh debug    # Build debug version
./setup.sh clean    # Clean build artifacts
./setup.sh rebuild  # Clean and rebuild
./setup.sh test     # Run unit tests
./setup.sh install  # Install to system (requires sudo)
```

### CMake Options

```bash
cmake -DLYNX_BUILD_TESTS=ON \
      -DLYNX_BUILD_BENCHMARKS=OFF \
      -DLYNX_USE_SIMD=ON \
      -DMPS_DIR=/path/to/mps \
      ..
```

## Usage

```cpp
#include <lynx/lynx.h>

int main() {
    // Create database configuration
    lynx::Config config;
    config.dimension = 128;
    config.index_type = lynx::IndexType::HNSW;
    config.distance_metric = lynx::DistanceMetric::L2;

    // Create database instance
    auto db = lynx::create_database(config);

    // Insert vectors
    std::vector<float> vec(128, 0.5f);
    db->insert(1, vec);

    // Search for nearest neighbors
    auto results = db->search(vec, 10);  // k=10

    for (const auto& [id, distance] : results) {
        std::cout << "ID: " << id << ", Distance: " << distance << "\n";
    }

    return 0;
}
```

## Project Structure

```
lynx_vector_db/
├── CMakeLists.txt       # Build configuration
├── setup.sh             # Build script
├── CLAUDE.md            # AI assistant instructions
├── CONCEPT.md           # Architecture and design
├── STATE.md             # Implementation status
├── README.md            # This file
├── LICENSE              # MIT License
├── doc/
│   └── research.md      # ANN algorithm research
├── src/
│   ├── include/
│   │   └── lynx/
│   │       └── lynx.h   # Public interface
│   ├── lib/
│   │   └── ...          # Implementation files
│   └── main.cpp         # Example executable
└── tests/
    └── ...              # Unit tests
```

## Architecture

Lynx uses a layered architecture:

1. **API Layer**: Pure virtual interface (`IVectorDatabase`)
2. **Index Layer**: HNSW, IVF implementations
3. **Threading Layer**: MPS pools and workers
4. **Storage Layer**: Memory-mapped persistence

See [CONCEPT.md](CONCEPT.md) for detailed design documentation.

## Performance

| Dataset Size | Index Type | Recall@10 | Query Latency (p99) |
|-------------|------------|-----------|---------------------|
| 1M vectors  | HNSW       | 0.95+     | < 5ms               |
| 10M vectors | HNSW       | 0.95+     | < 20ms              |
| 1B vectors  | HNSW       | 0.84-0.99 | 16-32ms             |

*Benchmarks based on research analysis. Actual performance may vary.*

## Documentation

- [CONCEPT.md](CONCEPT.md) - Design concept and implementation phases
- [STATE.md](STATE.md) - Current implementation state
- [doc/research.md](doc/research.md) - ANN algorithm research

## Contributing

1. Read [CLAUDE.md](CLAUDE.md) for coding guidelines
2. Check [STATE.md](STATE.md) for current progress
3. Follow the code style conventions
4. Add tests for new functionality

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- MPS library by Alexk-195
- HNSW algorithm research papers
- Vector database community
