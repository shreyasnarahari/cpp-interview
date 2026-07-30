# Theory & Mechanics: Low-Level Memory Layout & Optimization

## 1. Struct Padding & Alignment Rules

The compiler inserts **padding bytes** to satisfy alignment requirements. Each type must be stored at an address that is a multiple of its alignment:

```cpp
struct Bad {           // sizeof = 24 (with padding)
    char a;            // offset 0, size 1
    // 7 bytes padding (double requires 8-byte alignment)
    double b;          // offset 8, size 8
    char c;            // offset 16, size 1
    // 7 bytes padding (struct alignment = max member alignment = 8)
};

struct Good {          // sizeof = 16 (reordered to minimize padding)
    double b;          // offset 0, size 8
    char a;            // offset 8, size 1
    char c;            // offset 9, size 1
    // 6 bytes tail padding (struct total must be multiple of alignment = 8)
};
```

**Rule:** Sort members from **largest to smallest** alignment to minimize padding.

```
Alignment requirements (x86-64, typical):
  char:   1 byte    int:     4 bytes    pointer: 8 bytes
  short:  2 bytes   float:   4 bytes    double:  8 bytes
  
Struct alignment = max alignment of all members
Struct size = multiple of struct alignment (tail padding added if needed)
```

### `alignas` — Custom alignment:
```cpp
struct alignas(64) CacheAligned {  // aligned to cache line boundary
    int data;
    // 60 bytes padding to fill cache line
};
// sizeof(CacheAligned) == 64
// Every instance starts on a 64-byte boundary
```

## 2. Cache Hierarchy — The Latency Numbers

```
┌─────────────────────────────────────────────────┐
│ CPU Core                                        │
│ ┌──────────────────┐  ┌──────────────────┐      │
│ │ L1 Data   32 KB  │  │ L1 Instr  32 KB  │      │ ~1 ns  (~4 cycles)
│ └──────────────────┘  └──────────────────┘      │
│ ┌───────────────────────────────────────────┐   │
│ │ L2 Cache                    256 KB–1 MB   │   │ ~3-5 ns  (~12 cycles)
│ └───────────────────────────────────────────┘   │
└─────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────┐
│ L3 Cache (shared across cores)   8–32 MB        │ ~10-20 ns  (~40 cycles)
└─────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────┐
│ Main Memory (DRAM)               8–256 GB       │ ~50-100 ns  (~200 cycles)
└─────────────────────────────────────────────────┘

Key insight: L1 hit is ~100x faster than main memory access.
A cache miss on the hot path is catastrophic for latency-sensitive code.
```

### Cache line:
The CPU loads memory in **64-byte cache lines** — accessing any byte in a line loads the entire 64 bytes:
```
If you access arr[0] (4 bytes), the CPU loads bytes 0–63 into L1.
If your next access is arr[1] (bytes 4–7), it's already in cache — FREE.
If your next access is random_ptr->data (different cache line) — cache MISS (~50-100ns penalty).
```

## 3. False Sharing — Performance Trap

```cpp
// Two threads incrementing "independent" counters:
struct Counters {
    std::atomic<int> a;  // thread 1 writes this
    std::atomic<int> b;  // thread 2 writes this
};
// sizeof(Counters) = 8 bytes — both fit in ONE 64-byte cache line
// Every write by thread 1 invalidates thread 2's cache line and vice versa
// Result: ~100x slower than expected (constant MESI coherency traffic)

// FIX: pad to separate cache lines:
struct Counters {
    alignas(64) std::atomic<int> a;  // thread 1 — own cache line
    alignas(64) std::atomic<int> b;  // thread 2 — own cache line
};
// No false sharing — each thread's writes are isolated
```

**Detection:** Use `perf stat -e cache-misses,cache-references` or Intel VTune. If cache miss rate is high despite sequential access patterns, suspect false sharing.

## 4. VTable Mechanics — Object Memory Layout

```cpp
class Base {
    int x;
    virtual void foo();
    virtual ~Base();
};

// Object layout (64-bit system):
// Offset  Member
//   0     vptr (8 bytes) → points to Base's vtable
//   8     x (4 bytes)
//  12     padding (4 bytes)  — struct alignment = 8
// Total: 16 bytes

// Without virtual functions:
// Offset  Member
//   0     x (4 bytes)
// Total: 4 bytes
// Adding one virtual function costs 12 bytes per object (8 for vptr + padding)
```

### Virtual function call cost breakdown:
```
Direct call:     ~1 ns  (predicted branch, instruction in L1 icache)
Virtual call:    ~5 ns  (load vptr → load vtable entry → indirect call)
  Breakdown:
    1. Load vptr from object   ~1 cycle (L1 cache hit)
    2. Load fn ptr from vtable ~1 cycle (L1 cache hit, if hot)
    3. Indirect branch         ~3-5 cycles (may mispredict + icache miss)
```

## 5. Empty Base Optimization (EBO)

An empty class (no data members, no virtual functions) has `sizeof >= 1` (to ensure distinct addresses). But as a **base class**, the compiler can optimize its size to 0:

```cpp
struct Empty {};                          // sizeof(Empty) == 1
struct Holder { Empty e; int x; };        // sizeof(Holder) == 8 (1 + padding + 4)
struct Optimized : Empty { int x; };      // sizeof(Optimized) == 4 (EBO applied!)

// Real-world use: stateless allocators, comparators, deleters:
// std::unique_ptr<T, Deleter> stores Deleter as a compressed pair with T*
// If Deleter is empty (stateless lambda/functor), EBO eliminates its storage
```

C++20 introduces `[[no_unique_address]]` to enable EBO-like optimization for members:
```cpp
struct Holder {
    [[no_unique_address]] Empty e;  // compiler may give e zero size
    int x;
};
// sizeof(Holder) == 4  (with [[no_unique_address]])
```

## 6. SoA vs AoS — Memory Access Patterns

```cpp
// Array of Structs (AoS) — natural but cache-wasteful for column access:
struct Particle { float x, y, z; float vx, vy, vz; int type; };
std::vector<Particle> particles(1'000'000);

// Iterating over just positions loads velocity + type into cache too:
for (auto& p : particles) {
    p.x += p.vx * dt;  // touches 28 bytes per particle, uses 12
    p.y += p.vy * dt;   // cache efficiency: 12/28 ≈ 43%
    p.z += p.vz * dt;
}

// Struct of Arrays (SoA) — cache-friendly for column access:
struct Particles {
    std::vector<float> x, y, z;       // positions contiguous
    std::vector<float> vx, vy, vz;    // velocities contiguous
    std::vector<int> type;
};

// Iterating over positions touches ONLY position data:
for (size_t i = 0; i < n; i++) {
    p.x[i] += p.vx[i] * dt;  // touches exactly the bytes needed
    p.y[i] += p.vy[i] * dt;  // cache efficiency: ~100%
    p.z[i] += p.vz[i] * dt;  // hardware prefetcher loves sequential access
}
// Speedup: typically 2-5x for large datasets
```
