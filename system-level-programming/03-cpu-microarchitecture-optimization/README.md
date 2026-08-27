# Module 03: CPU Microarchitecture & Cache-Conscious C++

An advanced low-latency systems engineering reference on CPU execution pipelines, cache hierarchies, memory access patterns, branch prediction recovery, Array-of-Structures (AoS) vs Structure-of-Arrays (SoA), and AVX2 / AVX-512 SIMD vectorization.

---

## 1. CPU Execution Pipeline & Out-of-Order Execution (OoO)

Modern x86_64 processors (Intel Raptor Lake / AMD Zen 4) process instructions through an aggressive superscalar Out-of-Order pipeline.

```
+--------------------------------------------------------------------------------------+
| FRONTEND (In-Order)                                                                  |
|                                                                                      |
|  [Instruction Fetch] ---> [Branch Predictor (Tage / BTB)] ---> [Decode: x86 -> uOps] |
|                                                                         |            |
+-------------------------------------------------------------------------|------------+
                                                                          v
+--------------------------------------------------------------------------------------+
| ALLOCATION & REGISTER RENAMING                                                       |
|                                                                                      |
|  Eliminates False Dependencies (WAR / WAW hazards) using Physical Register File      |
|  Dispatches uOps to Reservation Stations (Scheduler) & Reorder Buffer (ROB)          |
+-------------------------------------------------------------------------|------------+
                                                                          v
+--------------------------------------------------------------------------------------+
| BACKEND EXECUTION ENGINE (Out-of-Order)                                              |
|                                                                                      |
|   Port 0/1 (ALU/Vector)   Port 2/3 (Load AGU)   Port 4 (Store Data)   Port 7/8 (Store AGU)
|   [FMA / Vector Math]     [L1 Cache Hit: ~4c]   [Store Buffer]        [Address Gen]  |
|                                                                                      |
+-------------------------------------------------------------------------|------------+
                                                                          v
+--------------------------------------------------------------------------------------+
| RETIREMENT & COMMIT (In-Order)                                                       |
|                                                                                      |
|  Reorder Buffer (ROB) ensures architectural state updates commit in program order    |
+--------------------------------------------------------------------------------------+
```

### Branch Prediction & Speculative Execution Penalties

When the branch predictor mispredicts a condition (e.g. unpredictable `if (x > threshold)` on market tick data):
1. Speculatively executed instructions in the pipeline must be **flushed** from the Reorder Buffer (ROB).
2. The frontend pipeline must be flushed and restarted from the correct target address.
3. **Misprediction Penalty**: **15 to 20 CPU cycles** of dead time.
4. **Branchless Programming**: Use conditional moves (`cmov`), bitwise masking, or mathematical formulation (`res = (val > 0) * val;`) to eliminate unpredictable branches on critical hot paths.

---

## 2. Memory Hierarchy & Cache Mechanics

Modern CPU registers compute at sub-nanosecond speeds ($\sim 0.2\text{ ns}$ / $5\text{ GHz}$), but DRAM operates at $\sim 60\text{--}100\text{ ns}$. The cache hierarchy bridges this $300\times$ speed gap.

| Cache Level | Typical Size | Latency (Cycles) | Latency (Time @ 4GHz) | Managed By |
|---|---|---|---|---|
| **CPU Registers** | ~2 KB (GPR + Vector) | 0–1 cycles | ~0.25 ns | Compiler |
| **L1 Data Cache (L1D)** | 32 KB – 48 KB | 4–5 cycles | ~1.0 ns | Hardware |
| **L2 Cache** | 512 KB – 1 MB | 12–14 cycles | ~3.0 ns | Hardware |
| **L3 Cache (LLC)** | 16 MB – 64 MB | 40–55 cycles | ~12.0 ns | Hardware |
| **Main Memory (DRAM)** | 32 GB – 512 GB | 200+ cycles | ~50–80 ns | Memory Controller |

### Cache Lines (64 Bytes) & Spatial vs Temporal Locality:
- Memory is transferred between DRAM and CPU caches strictly in **64-byte cache lines**.
- **Spatial Locality**: Accessing address $X$ automatically pulls $[X, X+63]$ into L1. Accessing adjacent bytes incurs 0 DRAM penalty.
- **Temporal Locality**: Data accessed recently is retained in L1/L2 for reuse.
- **Hardware Prefetchers**: Stream and stride prefetchers detect linear memory traversals and proactively fetch subsequent cache lines into L2/L1 before instruction execution. Random pointer chasing (e.g. node-based `std::list` or binary trees) breaks prefetchers and stalls the pipeline.

---

## 3. Data Layout: Array-of-Structures (AoS) vs Structure-of-Arrays (SoA)

Consider processing 10,000,000 market tick events to calculate Volume-Weighted Average Price (VWAP).

### Array-of-Structures (AoS) — Poor Cache Locality for Projections:
```cpp
struct MarketTick {
    double price;        // 8 bytes (Used in VWAP)
    uint32_t volume;     // 4 bytes (Used in VWAP)
    uint8_t flags;       // 1 byte  (Unused)
    // 3 bytes padding
    uint64_t timestamp;  // 8 bytes (Unused)
    char symbol[8];      // 8 bytes (Unused)
}; // Total: 32 bytes per tick
```
- In AoS, each 64-byte cache line holds only **2 ticks**.
- Only 12 bytes out of 32 bytes are needed ($37.5\%$ bandwidth efficiency). $62.5\%$ of memory bus throughput is wasted transferring unused fields.

### Structure-of-Arrays (SoA) — Optimal Cache Locality & SIMD Sympathy:
```cpp
struct MarketTickBatch {
    std::vector<double> prices;       // Contiguous doubles
    std::vector<uint32_t> volumes;    // Contiguous volumes
    std::vector<uint64_t> timestamps; // Isolated
    std::vector<uint8_t> flags;       // Isolated
};
```
- In SoA, a single 64-byte cache line holds **8 contiguous prices** or **16 contiguous volumes**.
- $100\%$ memory bandwidth utilization.
- Enables direct auto-vectorization and manual AVX2/AVX-512 vector register loading.

```
AoS Cache Line (64 Bytes):
+------------+------------+------------+------------+
| Tick 0 P/V | Tick 0 Aux | Tick 1 P/V | Tick 1 Aux |  <- 50% Wasted Bandwidth
+------------+------------+------------+------------+

SoA Cache Line (64 Bytes):
+------------+------------+------------+------------+------------+------------+------------+------------+
| Price 0    | Price 1    | Price 2    | Price 3    | Price 4    | Price 5    | Price 6    | Price 7    |
+------------+------------+------------+------------+------------+------------+------------+------------+
  <- 100% Useful Data for Math Kernel! Perfect for 256-bit SIMD Registers (_mm256_load_pd) ->
```

---

## 4. AVX2 Vectorization & Streaming Compute

Advanced Vector Extensions 2 (AVX2) introduces 256-bit wide registers (`%ymm0` – `%ymm15`), allowing simultaneous computation on:
- **4 parallel 64-bit double-precision floats** (`_mm256_add_pd`, `_mm256_mul_pd`, `_mm256_fmadd_pd`)
- **8 parallel 32-bit single-precision floats / integers** (`_mm256_add_ps`, `_mm256_add_epi32`)

### Non-Temporal Streaming Stores (`_mm256_stream_pd`):
When writing massive output datasets that will not be read again immediately, standard writes pollute L1/L2 caches and force a "Read-For-Ownership" (RFO) DRAM bus transaction. **Streaming stores** write directly to write-combining buffers and DRAM, bypassing cache entirely.

---

## 💻 Lab Exercises & Projects in this Module

1. **`exercises/market_tick.hpp` & `exercises/aos_vs_soa_simd.cpp`**:
   - Compares scalar AoS processing, scalar SoA processing, and AVX2 vectorized FMA SoA execution over 10,000,000 tick records.
2. **`benchmarks/bench_simd.cpp`**:
   - Nanosecond benchmarking measuring memory bandwidth, cycles per element, and throughput speedup.
3. **`projects/vectorized_math_pipeline/`**:
   - Production streaming vector compute engine with 32-byte aligned buffers, AVX2 fused multiply-add (`_mm256_fmadd_pd`), loop unrolling, and non-temporal streaming writes.
