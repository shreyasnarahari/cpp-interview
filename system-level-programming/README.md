# System-Level Programming & Low-Latency Systems Architecture

An advanced, production-grade engineering curriculum and laboratory designed for mastering low-level systems programming in modern C++ (C++20/C++23), Linux kernel interfaces, hardware architecture, mechanical sympathy, lock-free concurrency, and low-latency infrastructure.

---

## 🎯 Curriculum Philosophy & Scope

This module bridges high-level C++ software architecture and bare-metal execution mechanics. Rather than treating the operating system and CPU as black boxes, this curriculum explores how memory buses, cache coherency protocols, virtual memory managers, system calls, and compiler optimizers interact to achieve deterministic, nanosecond-scale performance.

### Core Learning Outcomes:
1. **Mechanical Sympathy**: Master CPU cache hierarchies (L1/L2/L3, cache line bouncing, false sharing, hardware prefetchers, MESI protocol) and pipeline execution (branch prediction, speculative execution, out-of-order retirement).
2. **Deterministic Memory Subsystems**: Implement custom memory allocators (monotonic arenas, intrusive free-lists, slab/buddy allocators, huge pages `mmap`) with zero runtime heap fragmentation.
3. **Lock-Free Concurrency**: Design wait-free and lock-free data structures (SPSC/MPMC queues, Hazard Pointers, Read-Copy-Update) using exact C++20 atomic memory models (`std::memory_order_relaxed`, `acquire`, `release`, `seq_cst`).
4. **Kernel-Bypass & Modern Asynchronous I/O**: Utilize modern Linux system programming primitives (`io_uring`, `epoll`, `splice`, `eventfd`, `futex`) to achieve zero-copy high-throughput communication.
5. **Real-Time Linux Tuning**: Master thread pinning (`sched_setaffinity`), isolcpus, real-time scheduling policies (`SCHED_FIFO`, `SCHED_DEADLINE`), non-uniform memory access (NUMA) node binding, and OS jitter elimination.
6. **Binary Analysis & Tooling**: Dissect ELF binaries, symbol tables, PLT/GOT relocations, and profile production hotpaths using Linux `perf`, PMU hardware performance counters, eBPF, and sanitizers (ASan, TSan, UBSan).

---

## 🗺️ Module Architecture & Roadmap

```
system-level-programming/
├── README.md                                         ← Master curriculum & architectural guide
├── CMakeLists.txt                                    ← Root CMake build system with Sanitizers
├── common/                                           ← Core low-latency infrastructure & utilities
│   ├── rdtsc_timer.hpp                               ← Cycle-accurate timer & latency distribution tracker
│   ├── logger.hpp                                    ← Zero-allocation lock-free diagnostic logger
│   └── memory_utils.hpp                              ← Cache alignment, hardware interference & hex dumper
│
├── 01-os-abi-and-syscall-internals/                  ← SysV ABI, Ring 3->0, ELF64, PLT/GOT, Bare-Metal -nostdlib
├── 02-virtual-memory-and-custom-allocators/          ← 4-Level Page tables, TLB, Page faults, Buddy & Slab allocators
├── 03-cpu-microarchitecture-optimization/            ← CPU OoO pipeline, Memory hierarchy, AoS vs SoA, AVX2 SIMD
├── 04-hardware-sync-and-lock-free-internals/         ← MESI, Memory models, Wait-free SPSC ring, Lock-free MPMC queue
├── 05-advanced-io-and-event-engines/                 ← Non-blocking I/O, Edge-Triggered epoll, Linux io_uring reactor
├── 06-systems-ipc-and-shared-memory/                 ← POSIX shared memory, Linux futex mutex, Sub-100ns SHM IPC
│
├── 07-database-storage-and-buffer-pool/              ← 4KB Slotted page layout, Buffer Pool Manager, LRU-K (k=2)
├── 08-database-indexing-and-concurrency/             ← B+ Tree Latch Crabbing, LSM-Tree Engine, MemTable & SSTables
├── 09-database-transactions-and-recovery/            ← MVCC Version Chains, Undo Logs, Snapshot Isolation, ARIES WAL
│
├── 10-signals-traps-and-runtime-control/             ← Linux signals, sigaltstack, ptrace, Async-signal-safe crash probe
├── 11-kernel-diagnostics-ebpf-profiling/             ← Hardware PMUs, Linux perf, eBPF Uprobes, Sanitizers internals
└── 12-capstone-systems/                              ← 3 Full-Scale Production Capstone Architectures
    ├── 01-shm-orderbook-feed/                        ← Ultra-Low Latency L2 Shared-Memory Orderbook & Tick-to-Trade
    ├── 02-embedded-lsm-kv-database/                  ← Production Embedded LSM KV Engine with io_uring WAL & Compaction
    └── 03-cooperative-fiber-scheduler/               ← High-Performance User-Space Cooperative Fiber Scheduler
```

---

## 🛠️ Toolchain Prerequisites & Environment

To compile and benchmark the modules with full hardware and kernel capabilities, the following development environment is recommended:

| Component | Minimum Version | Recommended / Target | Purpose |
|---|---|---|---|
| **Operating System** | Linux 5.15+ (x86_64) | Linux 6.x (Ubuntu 22.04 LTS+) | Modern kernel APIs (`io_uring`, `futex2`) |
| **Compiler** | GCC 13+ / Clang 17+ | GCC 13.2 / Clang 18 | Full C++20/C++23 support & sanitizers |
| **Build System** | CMake 3.25+ | CMake 3.28+ | Modern target-based build infrastructure |
| **`liburing`** | `liburing 2.3+` | `liburing-dev` | High-performance asynchronous kernel I/O |
| **`libnuma`** | `libnuma 2.0+` | `libnuma-dev` | NUMA memory node policy management |
| **`elfutils`** | `0.186+` | `libelf-dev` | Binary analysis and symbol table parsing |
| **Profiling Tools** | `linux-tools-generic` | `perf`, `valgrind`, `gdb` | Low-overhead PMU performance counters |

### Ubuntu / Debian Package Installation:
```bash
sudo apt-get update && sudo apt-get install -y \
    build-essential \
    cmake \
    g++-13 \
    clang-17 \
    liburing-dev \
    libnuma-dev \
    libelf-dev \
    linux-tools-generic \
    valgrind \
    gdb
```

---

## 🚀 Compilation & Build System

The build system is powered by target-based CMake with strict compiler flags (`-Wall -Wextra -Wpedantic -Wconversion -Wshadow -O3 -march=native -pthread`).

### Standard Release Build:
```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Sanitizer Builds (Verification & Safety):
Address & Undefined Behavior Sanitizer:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
cmake --build . -j$(nproc)
```

ThreadSanitizer (Data Race Detection):
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
cmake --build . -j$(nproc)
```

---

## 🧱 Shared Infrastructure Library (`common/`)

The `system-level-programming/common/` directory provides essential, header-only infrastructure for building low-latency C++ systems:

1. **[`rdtsc_timer.hpp`](./common/rdtsc_timer.hpp)**:
   - Cycle-accurate timing using serialized `rdtsc`/`rdtscp` (`_mm_lfence` / `cpuid`).
   - Automated CPU frequency calibration against `CLOCK_MONOTONIC_RAW`.
   - `LatencyTracker` for zero-allocation recording and calculation of p50, p99, p99.9, and p99.99 latency distributions.

2. **[`logger.hpp`](./common/logger.hpp)**:
   - High-throughput, zero-allocation lock-free diagnostic logging.
   - Microsecond & nanosecond timestamping.
   - Dedicated async-signal-safe crash reporting routines suitable for fatal signal handlers (`SIGSEGV`, `SIGBUS`, `SIGFPE`).

3. **[`memory_utils.hpp`](./common/memory_utils.hpp)**:
   - Cache-line alignment helpers (`alignas(64)`) and `hardware_destructive_interference_size` wrappers.
   - Formatted hex dump and struct memory alignment inspection tools.
   - Hardware cache flush (`clflush`) and prefetch intrinsics (`_mm_prefetch`).
