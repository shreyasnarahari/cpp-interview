# Low-Level Memory Layout & Performance Optimization — Deep Theory & Mechanics

An exhaustive, hardware-conscious engineering reference on CPU memory alignment, struct padding rules, hardware cache hierarchies, False Sharing, Array-of-Structures (AoS) vs Structure-of-Arrays (SoA), branch prediction, pointer aliasing (`__restrict__`), and cache-conscious data layouts.

---

## 1. Struct Layout, Alignment, and Padding Mechanics

### 1.1 Natural Alignment Rules
To maximize CPU memory bus efficiency, variables are aligned to memory addresses that are multiples of their size:
- `uint8_t` (1 Byte) $\implies$ aligned to $1$-byte boundary.
- `uint16_t` (2 Bytes) $\implies$ aligned to $2$-byte boundary.
- `uint32_t` (4 Bytes) $\implies$ aligned to $4$-byte boundary.
- `uint64_t` / Pointers (8 Bytes) $\implies$ aligned to $8$-byte boundary.

### 1.2 Struct Padding & Reordering Optimization

```
Unoptimized Struct (24 Bytes - 50% Padding Waste!):
struct Unoptimized {
    char a;      // 1 Byte
    // [Padding: 7 Bytes inserted by compiler!]
    double b;    // 8 Bytes
    int c;       // 4 Bytes
    // [Tail Padding: 4 Bytes to align total struct size to multiple of 8!]
};

Optimized Struct (16 Bytes - Zero Padding Waste!):
struct Optimized {
    double b;    // 8 Bytes (Offset 0x00)
    int c;       // 4 Bytes (Offset 0x08)
    char a;      // 1 Byte  (Offset 0x0C)
    // [Tail Padding: 3 Bytes] (Total: 16 Bytes)
};
```

**Optimization Rule**: Order struct members in **descending order of size** (8B $\to$ 4B $\to$ 2B $\to$ 1B) to minimize internal alignment padding.

---

## 2. Hardware Cache Hierarchies & Locality

```
+-------------------------------------------------------------------------------+
| L1 DATA CACHE (32 KB, ~4-5 CPU Cycles, Private per Core)                      |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| L2 CACHE (512 KB - 1 MB, ~14 CPU Cycles, Private per Core)                    |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| L3 CACHE (16 MB - 64 MB, ~40-60 CPU Cycles, Shared across all Cores)          |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| MAIN MEMORY (DRAM, ~150-250+ CPU Cycles)                                      |
+-------------------------------------------------------------------------------+
```

- **Cache Line Size**: **64 Bytes** on all modern x86-64 and ARM64 CPUs.
- When any byte is read, the hardware transfers the **entire 64-byte aligned block** into cache.

---

## 3. Array of Structs (AoS) vs Struct of Arrays (SoA)

```
Array of Structs (AoS) - Memory Layout:
[ (X0, Y0, Z0, Mass0) ][ (X1, Y1, Z1, Mass1) ][ (X2, Y2, Z2, Mass2) ]
-> Inefficient for vector arithmetic: Loading X brings unwanted Y, Z, Mass into cache!

Struct of Arrays (SoA) - Memory Layout:
[ X0, X1, X2, X3, ... ][ Y0, Y1, Y2, Y3, ... ][ Z0, Z1, Z2, ... ][ Mass0, Mass1, ... ]
-> Perfect for SIMD vectorization: Single AVX2 instruction loads 8 contiguous X values!
```

---

## 4. False Sharing & Cache Line Contention

False sharing occurs when independent threads update separate variables that happen to share the same 64-byte cache line, causing constant cache invalidations:

```cpp
// Prevention via C++17 hardware_destructive_interference_size:
struct alignas(std::hardware_destructive_interference_size) ThreadCounter {
    std::atomic<uint64_t> count{0};
};
```

---

## 5. Pointer Aliasing & The `__restrict__` Keyword

In standard C++, the compiler must assume that two pointers of the same type might point to overlapping memory. This forces the compiler to emit redundant memory loads:

```cpp
// WITHOUT restrict: Compiler must reload *b on every iteration in case *a modifies *b!
void add_vectors(int* a, int* b, int* c, int n) {
    for (int i = 0; i < n; ++i) {
        c[i] = a[i] + *b;
    }
}

// WITH __restrict__: Guarantees no pointer overlap -> Compiler hoists *b into CPU register!
void add_vectors_fast(int* __restrict__ a, int* __restrict__ b, int* __restrict__ c, int n) {
    for (int i = 0; i < n; ++i) {
        c[i] = a[i] + *b;
    }
}
```

---

## 6. Hot / Cold Data Splitting

Separates frequently accessed data from rarely accessed metadata to maximize L1/L2 cache capacity:

```cpp
// Cold data placed in separate struct via pointer:
struct OrderColdData {
    std::string client_notes;
    uint64_t routing_id;
    char compliance_flag;
};

// Hot data kept lean and cache-line aligned:
struct alignas(64) HotOrder {
    uint64_t order_id;
    double price;
    uint32_t quantity;
    char side;
    OrderColdData* cold_data; // Pointer to rare data
};
```
