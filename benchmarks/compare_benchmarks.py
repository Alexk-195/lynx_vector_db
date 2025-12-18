#!/usr/bin/env python3
"""
Benchmark comparison script for Lynx Vector DB vs ChromaDB.

This script runs both benchmarks and compares their performance on identical workloads:
- 10,000 vectors
- 512 dimensions
- HNSW index
- 10 search queries
"""

import subprocess
import sys
import os
import re
from pathlib import Path

def run_chromadb_benchmark():
    """Run ChromaDB benchmark and capture output."""
    print("=" * 60)
    print("Running ChromaDB Benchmark")
    print("=" * 60)

    script_dir = Path(__file__).parent
    chromadb_script = script_dir / "chromadb_test.py"

    if not chromadb_script.exists():
        print("Error: chromadb_test.py not found")
        return None

    try:
        result = subprocess.run(
            [sys.executable, str(chromadb_script)],
            capture_output=True,
            text=True,
            check=True
        )
        print(result.stdout)
        return parse_chromadb_output(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Error running ChromaDB benchmark: {e}")
        print(f"stderr: {e.stderr}")
        return None
    except Exception as e:
        print(f"Error: {e}")
        return None

def run_lynx_benchmark():
    """Run Lynx benchmark and capture output."""
    print("\n" + "=" * 60)
    print("Running Lynx Benchmark")
    print("=" * 60)

    script_dir = Path(__file__).parent
    lynx_script = script_dir / "run_lynx_benchmark.sh"

    if not lynx_script.exists():
        print("Error: run_lynx_benchmark.sh not found")
        return None

    try:
        result = subprocess.run(
            [str(lynx_script)],
            capture_output=True,
            text=True,
            check=True,
            cwd=script_dir
        )
        print(result.stdout)
        if result.stderr:
            print(result.stderr)
        return parse_lynx_output(result.stdout)
    except subprocess.CalledProcessError as e:
        print(f"Error running Lynx benchmark: {e}")
        print(f"stdout: {e.stdout}")
        print(f"stderr: {e.stderr}")
        return None
    except Exception as e:
        print(f"Error: {e}")
        return None

def parse_chromadb_output(output):
    """Parse ChromaDB benchmark output to extract metrics."""
    metrics = {
        'insert_time': None,
        'query_times': []
    }

    # Parse insertion time: "Inserted 10000 vectors in 1.23 seconds"
    insert_match = re.search(r'Inserted \d+ vectors in ([\d.]+) seconds', output)
    if insert_match:
        metrics['insert_time'] = float(insert_match.group(1))

    # Parse query times: "Query 1: 0.0123s, top IDs: ..."
    query_matches = re.finditer(r'Query \d+: ([\d.]+)s', output)
    for match in query_matches:
        metrics['query_times'].append(float(match.group(1)))

    return metrics

def parse_lynx_output(output):
    """Parse Lynx benchmark output to extract metrics."""
    metrics = {
        'insert_time': None,
        'query_times': []
    }

    # Parse insertion time: "Inserted 10000 vectors in 1.23 seconds"
    insert_match = re.search(r'Inserted \d+ vectors in ([\d.]+) seconds', output)
    if insert_match:
        metrics['insert_time'] = float(insert_match.group(1))

    # Parse query times: "Query 1: 0.0123s, top IDs: ..."
    query_matches = re.finditer(r'Query \d+: ([\d.]+)s', output)
    for match in query_matches:
        metrics['query_times'].append(float(match.group(1)))

    return metrics

def compare_metrics(chromadb_metrics, lynx_metrics):
    """Compare and display metrics from both databases."""
    print("\n" + "=" * 60)
    print("BENCHMARK COMPARISON RESULTS")
    print("=" * 60)

    if not chromadb_metrics or not lynx_metrics:
        print("Error: Could not collect metrics from one or both benchmarks")
        return

    # Compare insertion times
    print("\n📊 Insertion Performance (10,000 vectors, 512 dims)")
    print("-" * 60)
    if chromadb_metrics['insert_time'] and lynx_metrics['insert_time']:
        chromadb_time = chromadb_metrics['insert_time']
        lynx_time = lynx_metrics['insert_time']
        speedup = chromadb_time / lynx_time

        print(f"ChromaDB:  {chromadb_time:7.2f}s")
        print(f"Lynx:      {lynx_time:7.2f}s")

        if speedup > 1:
            print(f"\n✅ Lynx is {speedup:.2f}x FASTER")
        elif speedup < 1:
            print(f"\n⚠️  ChromaDB is {1/speedup:.2f}x faster")
        else:
            print(f"\n⚖️  Similar performance")

    # Compare query times
    print("\n📊 Query Performance (HNSW, k=5)")
    print("-" * 60)
    if chromadb_metrics['query_times'] and lynx_metrics['query_times']:
        chromadb_avg = sum(chromadb_metrics['query_times']) / len(chromadb_metrics['query_times'])
        lynx_avg = sum(lynx_metrics['query_times']) / len(lynx_metrics['query_times'])
        query_speedup = chromadb_avg / lynx_avg

        print(f"ChromaDB avg:  {chromadb_avg*1000:7.2f}ms")
        print(f"Lynx avg:      {lynx_avg*1000:7.2f}ms")

        if query_speedup > 1:
            print(f"\n✅ Lynx queries are {query_speedup:.2f}x FASTER")
        elif query_speedup < 1:
            print(f"\n⚠️  ChromaDB queries are {1/query_speedup:.2f}x faster")
        else:
            print(f"\n⚖️  Similar performance")

    # Detailed query comparison
    print("\n📊 Individual Query Times (ms)")
    print("-" * 60)
    print(f"{'Query':<10} {'ChromaDB':<12} {'Lynx':<12} {'Speedup':<10}")
    print("-" * 60)

    min_len = min(len(chromadb_metrics['query_times']), len(lynx_metrics['query_times']))
    for i in range(min_len):
        chroma_time = chromadb_metrics['query_times'][i] * 1000  # Convert to ms
        lynx_time = lynx_metrics['query_times'][i] * 1000        # Convert to ms
        speedup = chromadb_metrics['query_times'][i] / lynx_metrics['query_times'][i]

        speedup_str = f"{speedup:.2f}x" if speedup > 1 else f"1/{1/speedup:.2f}x"
        print(f"Query {i+1:<3}  {chroma_time:>8.2f} ms  {lynx_time:>8.2f} ms  {speedup_str:>9}")

    print("\n" + "=" * 60)

def main():
    print("""
╔════════════════════════════════════════════════════════════╗
║         Vector Database Benchmark Comparison               ║
║         Lynx vs ChromaDB                                   ║
╚════════════════════════════════════════════════════════════╝

Configuration:
  - Vectors:     10,000
  - Dimensions:  512
  - Index:       HNSW (M=32, ef_construction=200)
  - Metric:      L2 (Euclidean)
  - Queries:     10 (k=5 neighbors)
""")

    # Run benchmarks
    chromadb_metrics = run_chromadb_benchmark()
    lynx_metrics = run_lynx_benchmark()

    # Compare results
    compare_metrics(chromadb_metrics, lynx_metrics)

if __name__ == "__main__":
    main()
