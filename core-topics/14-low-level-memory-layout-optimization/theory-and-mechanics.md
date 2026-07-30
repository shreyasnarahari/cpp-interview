# Theory & Mechanics: Low-Level Memory Layout & Hardware Optimization

## 1. Hardware CPU Cache Hierarchy

Modern CPUs execute instructions in sub-nanosecond clock cycles, while reading from main RAM takes **~100–200 CPU cycles (Latency Bottleneck)**. To bridge this gap, hardware uses a tiered cache architecture:

```
Memory Hierarchy Speed & Capacity Spectrum:
┌─────────────────────────────────────────────────────────────┐
│ Registers          (64 x 64-bit)       0.5 cycles          │
├─────────────────────────────────────────────────────────────┤
│ L1 Data Cache      (32 KB per core)    1 - 4 cycles         │
├─────────────────────────────────────────────────────────────┤
│ L2 Cache           (512 KB per core)   10 - 14 cycles       │
├─────────────────────────────────────────────────────────────┤
│ L3 Cache (Shared)  (16 - 64 MB)        40 - 60 cycles       │
├─────────────────────────────────────────────────────────────┤
│ Main RAM (DDR4/5)  (Gigabytes)         150 - 250 cycles     │
└─────────────────────────────────────────────────────────────┘
```

### Cache Lines
Data is transferred between RAM and CPU caches in fixed 64-byte blocks called **Cache Lines**.
When you read a single `int32_t` (4 bytes), the hardware CPU pre-fetcher loads that byte **PLUS the surrounding 60 bytes** into the L1 cache.

---

## 2. Struct Alignment, Padding, & Ordering Rules

CPUs read memory efficiently when data addresses are aligned to multiples of their natural size (`alignof(T)`). The compiler inserts **padding bytes** between fields to ensure alignment.

### Unoptimized Struct Layout (16 Bytes):
```cpp
struct Unoptimized {
    char a;      // 1 byte
    // 3 bytes PADDING inserted by compiler!
    int b;       // 4 bytes (aligned to 4-byte boundary)
    char c;      // 1 byte
    // 3 bytes PADDING inserted to align struct size to 4-byte multiple!
}; // sizeof(Unoptimized) == 16 bytes!
```

### Optimized Struct Layout (8 Bytes):
By re-ordering fields from **largest alignment to smallest alignment**:

```cpp
struct Optimized {
    int b;       // 4 bytes
    char a;      // 1 byte
    char c;      // 1 byte
    // 2 bytes PADDING at end
}; // sizeof(Optimized) == 8 bytes! (50% MEMORY REDUCTION!)
```

### Empty Base Optimization (EBO) & `[[no_unique_address]]` (C++20)
In C++, an empty `struct` or `class` has `sizeof(Empty) == 1` byte so that distinct objects have distinct memory addresses.
- **Empty Base Optimization (EBO)**: If a class inherits from an empty base class, the compiler optimizes away the 1-byte overhead (`sizeof(Derived) == sizeof(int)`).
- **`[[no_unique_address]]` (C++20)**: Allows empty member variables (like stateless allocators or custom deleters) to occupy 0 bytes inside a class without inheritance!

---

## 3. Data-Oriented Design (DOD): AoS vs SoA

In performance-critical loops (HFT / Game Engines), Object-Oriented Array of Structs (AoS) causes cache pollution because loops iterate over a single field while dragging in unused bytes.

```
1. Array of Structs (AoS) Layout:
   [ ID | Price | Volume | Side ][ ID | Price | Volume | Side ] ...
   <- 32 Bytes loaded per order, only Volume (4B) used -> Waste 87% L1 Cache!

2. Struct of Arrays (SoA) Layout:
   Volumes: [ Vol0 | Vol1 | Vol2 | Vol3 | Vol4 | Vol5 ... ]
   <- Single 64B Cache Line holds 16 CONSECUTIVE VOLUMES! 100% L1 Cache Utilization!
```

### Hot/Cold Data Splitting Pattern
Separate frequently accessed "hot" fields (physics, position, volume) from infrequently accessed "cold" fields (names, descriptions, UI metadata):

```cpp
// Cold Metadata (Allocated separately on heap)
struct ObjectMetadata {
    std::string name;
    std::string description;
};

// Hot Storage (Extremely compact for 100% cache line density)
struct GameObject {
    float x, y, z;        // Position (Hot)
    float vx, vy, vz;     // Velocity (Hot)
    std::unique_ptr<ObjectMetadata> cold_meta; // Pointer to cold data
};
```

---

## 4. Hardware False Sharing Mitigation

When 2 threads running on Core 0 and Core 1 write to distinct atomic variables located within the same 64-byte cache line, the CPU cache hardware forces continuous invalidation across core L1 caches (**Cache Line Bouncing / False Sharing**).

### Fix via 64-Byte Cache Alignment:
```cpp
#include <new>

struct alignas(64) ThreadSafeCounter {
    std::atomic<uint64_t> value{0};
}; // Guaranteed to occupy its own isolated 64-byte cache line!
```
