import chromadb
import numpy as np
import time

NUM_VECTORS = 10_000
DIMENSION = 512
NUM_QUERIES = 10
BATCH_SIZE = 1000  # must be <= max batch size

client = chromadb.Client()
collection = client.create_collection(name="test_vectors",
    metadata={
        "hnsw:space": "l2",           # l2 | cosine | ip
        "hnsw:M": 32,                 # graph degree (default ~16)
        "hnsw:construction_ef": 200   # ef parameter during construction
    })

# Generate data
vectors = np.random.rand(NUM_VECTORS, DIMENSION).astype(np.float32)
ids = [f"vec_{i}" for i in range(NUM_VECTORS)]

print("Adding vectors in batches...")
start = time.time()

for i in range(0, NUM_VECTORS, BATCH_SIZE):
    collection.add(
        ids=ids[i:i+BATCH_SIZE],
        embeddings=vectors[i:i+BATCH_SIZE].tolist()
    )

print(f"Inserted {NUM_VECTORS} vectors in {time.time() - start:.2f} seconds")

# Queries
print("\nRunning queries...")
for i in range(NUM_QUERIES):
    q = np.random.rand(DIMENSION).astype(np.float32)
    t0 = time.time()

    res = collection.query(
        query_embeddings=[q.tolist()],
        n_results=5
    )

    print(f"Query {i+1}: {(time.time() - t0):.4f}s, top IDs: {res['ids'][0]}")
