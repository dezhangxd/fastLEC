# fastLEC

**fastLEC** is a parallel Logic Equivalence Checking (LEC) prover for AIGER datapath circuits, with multiple solving engines and sweeping strategies.

---

## Table of Contents

- [Build](#build)
- [Usage](#usage)
- [Input and Output](#input-and-output)
- [Modes](#modes)
- [Features](#features)
- [Dependencies and Requirements](#dependencies-and-requirements)
- [Project Structure](#project-structure)
- [Clean and Rebuild](#clean-and-rebuild)
- [License and Author](#license-and-author)

---

## Build

Clone the repository with submodules, then build from the project root:

```bash
git clone https://github.com/dezhangxd/fastLEC.git --recurse-submodules
cd fastLEC
./build.sh
```

The executable is `build/bin/fastLEC`. If submodules were not initialized:

```bash
git submodule update --init --recursive
```

**Build variants:**

| Command | Description |
|---------|-------------|
| `./build.sh` | Default: no CUDA, Release |
| `./build.sh no-cuda` | Disable CUDA (same as default) |
| `./build.sh cuda` | Enable CUDA and GPU-ES |
| `./build.sh debug` | No CUDA, Debug build |
| `./build.sh sanitize` | No CUDA, Address + Undefined sanitizers |

**Requirements:** CMake 3.16+, C++17 compiler (GCC or Clang), Make and autoconf/automake for submodules. For GPU-ES, install CUDA Toolkit and set `CUDA_HOME` or `CUDA_PATH` if needed.

---

## Usage

```text
fastLEC -i <input.aig> [options]
```

**Required:** `-i, --input <filename>` — path to the AIGER file.

**Options:**

| Option | Description | Default |
|--------|-------------|---------|
| `-h, --help` | Show help | — |
| `--modes` | List all run modes | — |
| `-m, --mode <mode>` | Run mode | ES |
| `-c, --cores <num>` | Number of cpu threads | 1 |
| `-t, --timeout <sec>` | Timeout in seconds | 30.0 |
| `-v, --verbose <level>` | Verbosity level | 1 |
| `-p, --param <key> <val>` | Custom parameter (use `--help` for list) | — |

**Examples:**

```bash
./build/bin/fastLEC -i ./data/test_16_TOP11.aiger -m ES -t 60
./build/bin/fastLEC -i ./data/test_16_TOP11.aiger -m hybrid_sweeping -c 8 -t 300 
./build/bin/fastLEC -i ./data/test_16_TOP11.aiger -m schedule_sweeping -c 8 -t 300   # requires from the project root directory
./build/bin/fastLEC -i ./data/test_16_TOP11.aiger -m gpu_sweeping -c 8 -t 120   # requires CUDA build
```

---

## Input and Output

**Input:** One AIGER file (`.aig` or `.aag`) given with `-i`. The tool checks the circuit for constant output or performs equivalence checking as implied by the mode.

**Output (stdout):**

- **Result line** (SMT/LEC-style):
  - `s Equivalent.` — circuits are equivalent
  - `s Not Equivalent.` — not equivalent (counterexample found)
  - `s Unknown.` — timeout or inconclusive
- **Comment lines:** prefixed with `c `, e.g. `c Runtime: 12.34 seconds` and other stats.

---

## Modes

Run `./build/bin/fastLEC --modes` to list all modes. Main modes:

### Single-engine

The whole circuit is checked by one engine without decomposition.

| Mode | Description |
|------|-------------|
| **ES** | Exhaustive Simulation; default, single-threaded. |
| **pES** | Multi-threaded exhaustive simulation. |
| **BDD** | BDD (CUDD) for equivalence checking, single-threaded. |
| **pBDD** | BDD (Sylvan) for equivalence checking, multi-threaded. |
| **SAT** | SAT solver (Kissat) only for equivalence checking. |
| **pSAT** | Multi-threaded SAT (multiple Kissat instances). |
| **gpuES** | Exhaustive simulation on GPU; requires CUDA build. |

### Sweeping (decomposition + sub-problems)

The circuit is decomposed into sub-problems; each sub-problem is solved with one or more engines.

| Mode | Description |
|------|-------------|
| **SAT_sweeping** | Each sub-problem solved with SAT (Kissat). |
| **BDD_sweeping** | Each sub-problem solved with BDD (CUDD). |
| **pSAT_sweeping** | Each sub-problem solved with multi-threaded SAT. |
| **pBDD_sweeping** | Each sub-problem solved with multi-threaded BDD (Sylvan). |
| **ES_sweeping** | Each sub-problem solved with ES. |
| **pES_sweeping** | Each sub-problem solved with pES. |
| **hybrid_sweeping** | Heuristically selects one serial engine (ES or SAT) per sub-problem. |
| **p_hybrid_sweeping** | Same as above with pES or pSAT, multi-threaded. |
| **half_sweeping** | Portfolio: about half of threads to SAT, the remainder to ES (and at most one thread to BDD). |
| **PPE_sweeping** | Parallel Portfolio Engine: heuristic rules (e.g. PI size, cost) allocate SAT/ES/BDD threads (no ML). |
| **schedule_sweeping** | ML (XGBoost)-scheduled portfolio: predicts solving time from circuit features and allocates SAT/ES/BDD threads. |
| **gpu_sweeping** | Portfolio including GPU-ES (GPU thread allocation); requires CUDA build. |

**Recommendation:** Default is **ES**. For harder instances, try **hybrid_sweeping** (1 CPU thread), **schedule_sweeping** (multiple CPU cores), or **gpu_sweeping** (when GPU is available).

---

## Features

- **Multiple engines:** SAT (Kissat), BDD (CUDD / Sylvan), ES (Exhaustive Simulation), GPU-ES (optional).
- **Parallelism and sweeping:** Multi-threaded solving and various sweeping strategies.
- **AIGER in, internal formats:** AIG, XAG, CNF with conversions.
- **Optional CUDA** for GPU-ES; **XGBoost** for engine selection in modes like `schedule_sweeping` and `PPE_sweeping`.

---

## Dependencies and Requirements

Submodules (use `--recurse-submodules` when cloning):

| Submodule | Path | Purpose |
|-----------|------|---------|
| XGBoost | `deps/xgboost` | ML for engine selection |
| AIGER | `deps/aiger` | AIGER format |
| Kissat | `deps/kissat` | SAT solver |
| CUDD | `deps/cudd` | Sequential BDD |
| Sylvan | `deps/sylvan` | Parallel BDD |

**Requirements:** CMake 3.16+, C++17, Make, autoconf/automake. Optional: CUDA Toolkit for GPU-ES.

---

## Project Structure

```text
fastLEC/
├── CMakeLists.txt   build.sh   clean.sh   LICENSE   README.md
├── deps/            # Submodules: aiger, cudd, kissat, sylvan, xgboost
├── src/             # Main sources (fastLEC.cpp, AIG, XAG, CNF, sweeper, ...)
├── include/
└── build/           # Created by build.sh; executable at build/bin/fastLEC
```

---

## Clean and Rebuild

```bash
./clean.sh
./build.sh
```

`clean.sh` removes the root `build/` and cleans submodule build dirs (xgboost, kissat, aiger, cudd, sylvan).

---

## License and Author

- **License:** MIT (see [LICENSE](LICENSE)).
- **Corresponding Author:** Xindi Zhang — dezhangxd96@gmail.com  
- **Repository:** https://github.com/dezhangxd/fastLEC  

Submodules have their own licenses; see each module’s LICENSE file.
