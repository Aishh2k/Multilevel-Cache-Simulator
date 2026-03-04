# Multilevel Cache Simulator

A configurable two-level (L1/L2) cache simulator written in C++11, built for the Georgia Tech HPCA course (CS 6290 / ECE 6100).

## Overview

This simulator models a realistic memory hierarchy with:
- **L1 Cache** — set-associative, LRU replacement, write-back with write-allocate
- **L2 Cache** — set-associative, LRU replacement, configurable prefetcher
- **Prefetching strategies** — None, Plus-One, Markov predictor, and Hybrid (Markov + Plus-One fallback)

## Project Structure

```
.
├── cachesim.cpp          # Core simulation logic (L1/L2 access, Markov table, prefetcher)
├── cachesim.hpp          # Data structures and configuration types
├── cachesim_driver.cpp   # Entry point: argument parsing and stats output
├── Makefile              # Build system
├── run.sh                # Convenience script to run the simulator
├── run_experiments.py    # Python script to sweep configs and collect results
├── validate_common.sh    # Shared validation helpers
├── validate_undergrad.sh # Validation for undergrad config set
├── validate_grad.sh      # Validation for grad config set
├── ref_outs/             # Reference outputs for validation
└── 6290docker-*.sh / .bat # Docker setup scripts (Linux/macOS/Windows)
```

## Build

```bash
make          # standard debug build
make FAST=1   # optimized build (-O2)
make SANITIZE=1  # build with AddressSanitizer
make clean    # remove binaries and object files
```

## Usage

```
./cachesim <l1_c> <l1_b> <l1_s> <l2_c> <l2_b> <l2_s> <prefetch_algo> <trace_file>
```

| Argument | Description |
|---|---|
| `l1_c` | Log₂ of L1 cache size in bytes |
| `l1_b` | Log₂ of L1 block size in bytes |
| `l1_s` | Log₂ of L1 associativity |
| `l2_c` | Log₂ of L2 cache size (set 0 to disable L2) |
| `l2_b` | Log₂ of L2 block size |
| `l2_s` | Log₂ of L2 associativity |
| `prefetch_algo` | `0` = None, `1` = Plus-One, `2` = Markov, `3` = Hybrid |
| `trace_file` | Memory access trace file path |

**Example:**
```bash
./cachesim 14 6 2 20 6 4 2 traces/gcc.trace
```

## Validation

```bash
make validate_undergrad   # run undergrad test suite
make validate_grad        # run grad test suite
```

## Prefetcher Details

| Mode | Description |
|---|---|
| **None (0)** | No prefetching |
| **Plus-One (1)** | Always prefetches the next sequential block into L2 on an L2 miss |
| **Markov (2)** | Builds a Markov transition table from L2 miss history; prefetches the most likely next block |
| **Hybrid (3)** | Uses Markov predictor; falls back to Plus-One if no Markov prediction is available |

## Docker (Optional)

Docker scripts are provided for a consistent build environment:
```bash
# macOS
bash 6290docker-macos.sh

# Linux
bash 6290docker-linux.sh

# Windows
6290docker.bat
```

## Stats Output

After simulation, the following metrics are reported:
- L1 hit/miss ratio and average access time
- L2 read hit/miss ratio and average access time
- Prefetch hit/miss counts (if prefetching enabled)
