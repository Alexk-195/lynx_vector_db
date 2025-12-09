# Lynx Vector Database

A high-performance vector database implemented in modern C++20 with support for Approximate Nearest Neighbor (ANN) search algorithms.

## Features

- **HNSW Index**: Hierarchical Navigable Small World graphs for O(log N) query time
- **Multi-threaded**: Built on MPS (Message Processing System) for concurrent operations
- **Modern C++20**: Concepts, spans, ranges, and coroutines
- **Flexible Distance Metrics**: L2 (Euclidean), Cosine, Dot Product

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

For minimal example see [src/main_minimal.cpp](src/main_minimal.cpp)

## Project Structure

```
lynx_vector_db/
├── Makefile             # Build configuration
├── setup.sh             # Build script
├── CLAUDE.md            # AI assistant instructions
├── CONCEPT.md           # Architecture and design
├── STATE.md             # Implementation status
├── TODOS.md             # Current Todos to implement
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
│   └── main_minimal.cpp         # Example of minimal database executable
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

- [CONCEPT.md](CONCEPT.md) - Design concept and implementation phases
- [STATE.md](STATE.md) - Current implementation state
- [doc/research.md](doc/research.md) - ANN algorithm research
- [tests/README.md](tests/README.md) - Infos about unit testing

## Contributing

1. Read [CLAUDE.md](CLAUDE.md) for coding guidelines
2. Check [STATE.md](STATE.md) for current progress
3. Follow the code style conventions
4. Add tests for new functionality

## License

MIT License - see [LICENSE](LICENSE) for details.

