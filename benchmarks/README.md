# Benchmarks

This folder contains benchmark scripts to compare Lynx Vector Database performance against other vector database implementations.

## Available Benchmarks

### ChromaDB vs Lynx

Compare Lynx against ChromaDB on identical workloads.

**Test Configuration:**
- Vectors: 10,000
- Dimensions: 512
- Index: HNSW (M=32, ef_construction=200)
- Distance Metric: L2 (Euclidean)
- Queries: 10 searches for k=5 nearest neighbors

**Files:**
- `chromadb_test.py` - ChromaDB benchmark script
- `lynx_test.cpp` - Lynx benchmark application
- `run_lynx_benchmark.sh` - Build and run Lynx benchmark
- `compare_benchmarks.py` - Run both and compare results

### FAISS vs Lynx

Compare Lynx against FAISS (Facebook AI Similarity Search) library using HNSW index.

**Test Configuration:**
- Vectors: 10,000
- Dimensions: 512
- Index: HNSW (M=32, ef_construction=200, ef_search=200)
- Distance Metric: L2 (Euclidean)
- Queries: 10 searches for k=5 nearest neighbors

**Files:**
- `faiss_test.cpp` - FAISS HNSW benchmark application
- `run_faiss_benchmark.sh` - Build and run FAISS benchmark

## Prerequisites

### For ChromaDB Benchmark

Install ChromaDB:
```bash
pip install chromadb numpy
```

### For Lynx Benchmark

Build Lynx first:
```bash
cd /home/alex/lynx_vector_db
./setup.sh
```

### For FAISS Benchmark

Install FAISS library. Choose one of the following methods:

**Option 1: Install via Conda (Recommended)**
```bash
# For CPU-only version
conda install -c pytorch faiss-cpu

# For GPU support
conda install -c pytorch faiss-gpu
```

**Option 2: Build from Source**
```bash
git clone https://github.com/facebookresearch/faiss.git
cd faiss
cmake -B build -DFAISS_ENABLE_GPU=OFF -DFAISS_ENABLE_PYTHON=OFF .
cmake --build build --parallel
sudo cmake --install build
```

**Option 3: System Package (Ubuntu/Debian)**
```bash
# Note: May not be available on all distributions
sudo apt-get install libfaiss-dev
```

## Running Benchmarks

### Run Individual Benchmarks

**ChromaDB only:**
```bash
cd benchmarks
python chromadb_test.py
```

**Lynx only:**
```bash
cd benchmarks
./run_lynx_benchmark.sh
```

**FAISS only:**
```bash
cd benchmarks
./run_faiss_benchmark.sh
```

### Run Comparison

Run both benchmarks and compare results:
```bash
cd benchmarks
./compare_benchmarks.py
```

This will:
1. Run ChromaDB benchmark
2. Run Lynx benchmark
3. Display side-by-side comparison
4. Show performance metrics and speedup

## Expected Output

The comparison script will show:

- **Insertion Performance**: Time to insert 10,000 vectors
- **Query Performance**: Average query latency
- **Individual Query Times**: Per-query comparison
- **Speedup Metrics**: Performance multipliers

Example output:
```
📊 Insertion Performance (10,000 vectors, 512 dims)
------------------------------------------------------------
ChromaDB:     2.34s
Lynx:         1.12s

✅ Lynx is 2.09x FASTER

📊 Query Performance (HNSW, k=5)
------------------------------------------------------------
ChromaDB avg:    15.23ms
Lynx avg:         8.45ms

✅ Lynx queries are 1.80x FASTER
```

## Customizing Benchmarks

To modify test parameters, edit the constants in the benchmark files:

**chromadb_test.py:**
```python
NUM_VECTORS = 10_000
DIMENSION = 512
NUM_QUERIES = 10
BATCH_SIZE = 1000
```

**lynx_test.cpp:**
```cpp
constexpr int NUM_VECTORS = 10'000;
constexpr int DIMENSION = 512;
constexpr int NUM_QUERIES = 10;
constexpr int BATCH_SIZE = 1000;
```

**faiss_test.cpp:**
```cpp
constexpr int NUM_VECTORS = 10'000;
constexpr int DIMENSION = 512;
constexpr int NUM_QUERIES = 10;
constexpr int BATCH_SIZE = 1000;
```

Remember to use the same values in all files for fair comparison.

## Notes

- All benchmarks use random data
- Results may vary based on system resources
- For production comparisons, use realistic datasets
- All benchmarks use similar HNSW parameters for fairness (M=32, ef_construction=200, ef_search=200)
- FAISS is a highly optimized library from Facebook Research, providing a good baseline for comparison
