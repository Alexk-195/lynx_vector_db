# Claude Code Instructions for Lynx Vector Database

## Project Overview

Lynx is a high-performance vector database implemented in modern C++20, using the MPS (Message Processing System) library for multi-threading.

## Required Reading

Before making changes, read these documents:
- `CONCEPT.md` - Architecture and design phases
- `README.md` - Project overview and build instructions
- `tickets/README.md` - Ticketing system for tracking tasks

## Code Style Guidelines

### Modern C++ (C++20)
- Use `std::span` for non-owning array views
- Use concepts for template constraints
- Prefer `std::expected` or `std::optional` for error handling
- Use RAII and smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- Prefer `constexpr` where possible
- Use structured bindings and range-based for loops

### Naming Conventions
- Classes: `PascalCase` (e.g., `VectorIndex`, `HNSWGraph`)
- Functions/methods: `snake_case` (e.g., `insert_vector`, `search_neighbors`)
- Member variables: `snake_case_` with trailing underscore (e.g., `dimension_`)
- Constants: `kPascalCase` (e.g., `kDefaultEfSearch`)
- Namespaces: `lowercase` (e.g., `lynx`, `lynx::index`)

### File Organization
- Headers in `src/include/lynx/`
- Implementation in `src/lib/`
- Main executable in `src/main.cpp`
- Tests in `tests/`

### Interface Design
- Public interfaces use pure virtual classes (abstract base classes)
- Implementation classes are internal and not exposed in public headers
- Use the pImpl idiom where ABI stability is needed

## Build Commands

```bash
./setup.sh          # Build release
./setup.sh debug    # Build debug
./setup.sh clean    # Clean build
./setup.sh test     # Run tests
```

## MPS Integration

The project uses MPS for threading. Key concepts:
- `mps::pool` - Thread + message queue
- `mps::worker` - Processes messages via `process()` method
- `mps::message` - Base class for all messages
- `mps::synchronized<T>` - Thread-safe data wrapper

Set `MPS_DIR` environment variable to point to MPS library location.

## Testing

- Unit tests should cover all public interface methods
- Benchmark tests for performance-critical code
- Use descriptive test names that explain the scenario

## Documentation

- Document public APIs with Doxygen-style comments
- Keep `CONCEPT.md` updated if architecture changes
- Update tickets and move completed work to `tickets/done/`
