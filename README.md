# Parallel Shortest Paths

High-performance C++20 playground for **single-source shortest path (SSSP)** algorithms with a strong focus on *parallel* delta-stepping variants.

The code base provides:

* A clean, header-only abstraction (`ShortestPathSolverBase`) to plug in new solvers.
* A sequential reference implementation (`Dijkstra`).
* Delta-stepping – sequential and three highly-optimised parallel variants that rely on lock-free / fine-grained data structures.
* An extensible benchmark driver that produces CSV summaries and pretty console output.
* A large-scale graph generator (uniform & power-law weights) to stress-test the algorithms.

> The project was developed as the code companion of the paper *"Completely Balanced Delta-Stepping for Massive Graphs"* (see `Parallel_Shortest_Paths.pdf`).

---

## 1. Requirements

| Requirement | Tested version |
|-------------|----------------|
| C++ compiler | **g++ 12** / **clang++ 15** with `-std=c++20` and POSIX threads |
| Make | GNU Make ≥ 4 |
| RAM | ≥ 16 GB (≥ 64 GB recommended to replay the huge benchmarks) |

On Ubuntu / WSL:
```bash
sudo apt update && sudo apt install build-essential
```

On Windows you may use **MSYS2/MinGW** or **WSL**. The supplied *Makefile* assumes `g++` – adjust if you want to use MSVC.

---

## 2. Building

```bash
# From the project root
make        # builds *main*  *graph_generator*  *benchmark*
```

Individual targets:

* `make main` – correctness test runner
* `make graph_generator` – large-scale graph generator
* `make benchmark` – benchmarking CLI
* `make clean` – wipe all objects & binaries

All artefacts are placed in the project root (`main`, `graph_generator`, `benchmark`). Object/dependency files live under `src/` as specified in the *Makefile*.

---

## 3. Directory layout (abridged)

```
.
├── src/
│   ├── core/     # generic graph & solver interfaces
│   ├── algo/     # algorithm implementations
│   ├── ds/       # lock-free queues / stacks / pools used by the parallel algos
│   └── tests/    # drivers, benchmark tool, generator & utilities
├── assets/
│   └── test_cases/  # (optionally) pre-generated graphs (can be hundreds of MB-GB!)
├── Makefile
└── Parallel_Shortest_Paths.pdf  # accompanying paper
```

---

## 4. Quick start

### 4.1 Correctness tests
```bash
./main
```
Runs an extensive battery of random & structured graphs and checks that every parallel solver returns the same distances as the sequential `Dijkstra` baseline. Expected output snippet:
```
Parallel Shortest Paths - Algorithm Testing
===========================================
=== Delta Stepping Parallel Correctness Tests ===
...
All solvers: PASS
```

### 4.2 Generate huge test graphs (optional)
```bash
./graph_generator
```
Produces ⚠ **very large** graphs (hundreds of MB to multiple GB) in `assets/test_cases/`. Each file is a simple text list of weighted directed edges
```
<u> <v> <w>   # one edge per line, 0-indexed vertices, <w> ∈ [0,1)
```
Generation parameters (sizes, distributions) are hard-coded in `src/tests/graph_generator.cpp` – adjust as you wish.

### 4.3 Benchmarking
```bash
# Benchmark every .txt in assets/test_cases using 5 repetitions
./benchmark --runs 5

# Benchmark a specific graph with 10 repetitions
./benchmark --runs 10 path/to/graph.txt
```
The tool walks through a matrix of delta values and thread counts, measures **min/avg/max** runtime, computes speed-up vs. `Dijkstra`, efficiency, and dumps the whole table to `benchmark_results.csv`.

---

## 5. Command-line reference

| Executable | Purpose | Synopsis |
|------------|---------|----------|
| `main` | correctness regression suite | `./main` |
| `graph_generator` | generate scaled graph instances | `./graph_generator` |
| `benchmark` | performance measurement harness | `./benchmark [--runs N] [graph1 graph2 ...]` |

---

## 6. Graph file format

A graph is an *edge list* of space-separated triples
```
<source> <target> <weight>
```
Vertices are **0-indexed integers**, weights are *double*s. When treating the graph as **undirected** we simply insert reciprocal edges.

---

## 7. Reproducing the paper plots

1. Build everything 👉 `make`
2. Generate or download the graphs into `assets/test_cases/`
3. Run the benchmark 👉 `./benchmark --runs 3`
4. Post-process `benchmark_results.csv` with the provided notebooks (`parallel_delta_stepping_analysis.ipynb`) or your favourite plotting library.

> The committed `*.csv` files already contain example runs on various graph classes (grid, RMAT, road-like, …) so you can jump straight to plotting.

---

## 8. Performance notes

* Set the environment variable `OMP_PLACES=cores` (or pin threads manually) to get stable numbers on NUMA machines.
* The parallel algorithms rely on *lock-free stacks/queues* plus a fixed-size thread pool that uses `std::barrier` – available only since C++20.
* For maximum performance compile **with `-O3 -march=native`** (edit the Makefile).

---

## 9. Troubleshooting

* *Make complains about missing `<barrier>* – you need GCC ≥ 11 or Clang ≥ 13.
* *Program crashes with `std::bad_alloc`* – you ran out of memory, use smaller graphs.
* *CSV contains `FAIL` in the `Correct` column* – the algorithm produced wrong distances; please file a bug with the failing `failed.txt` graph that is automatically written.
