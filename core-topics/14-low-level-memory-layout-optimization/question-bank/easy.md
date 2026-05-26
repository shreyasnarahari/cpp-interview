# Low-Level Memory Layout & Optimization — Easy Questions

---

**Q1. What is the output of `sizeof` for the following structs on a standard 64-bit architecture?**

```cpp
struct A {
    char c;
    int i;
    short s;
};

struct B {
    int i;
    short s;
    char c;
};
```

A:
- `sizeof(A)` is **12 bytes**.
- `sizeof(B)` is **8 bytes**.

**Explanation**: 
To ensure fast memory access, the CPU requires variables to be aligned in memory based on their size (e.g., a 4-byte `int` must start at a memory address divisible by 4). To enforce this, the compiler introduces hidden **padding** bytes. Furthermore, the struct itself must be padded at the end so that in an array of these structs, the next element starts at the correct alignment boundary (the alignment of the largest member).

- In `A`: `c` (1 byte) + *3 bytes padding* + `i` (4 bytes) + `s` (2 bytes) + *2 bytes tail padding* = 12 bytes. (Aligned to 4).
- In `B`: `i` (4 bytes) + `s` (2 bytes) + `c` (1 byte) + *1 byte tail padding* = 8 bytes.

**Rule of thumb**: Always order struct members from strictly largest to smallest to completely eliminate internal padding.

---

**Q2. What do `alignas` and `alignof` do?**

A: 
- `alignof(T)` returns the alignment requirement of type `T` in bytes. For example, `alignof(double)` is typically 8.
- `alignas(N)` forces a specific, stricter alignment for a variable or struct. It tells the compiler "Make sure this object starts at a memory address that is a multiple of N."

```cpp
struct alignas(64) CacheLineAlignedStruct {
    int data[4];
};
```
This guarantees the struct aligns perfectly with a typical 64-byte CPU cache line, which is crucial for preventing false sharing in concurrent code.

---

**Q3. What is `#pragma pack(1)` or `__attribute__((packed))`? Should you use it?**

A: It is a compiler directive instructing the compiler to **disable all padding**. If applied to `struct A` from Q1, its size would become exactly 7 bytes.

**Should you use it?** Almost never in performance-critical code. While it saves memory, it causes **Unaligned Memory Access**. When the CPU attempts to read a 4-byte integer that crosses a word boundary (e.g., spans across two 4-byte chunks of memory), it takes significantly more CPU cycles to load, and on some architectures (like ARM), unaligned access triggers a hardware fault and crashes the program. It is only used for network packet serialization or strict hardware protocol mapping.

---

**Q4. What are bit-fields?**

A: Bit-fields allow you to specify exactly how many *bits* a variable should consume.

```cpp
struct Flags {
    unsigned int isVisible : 1;
    unsigned int isStatic  : 1;
    unsigned int layer     : 6;
};
```
This struct packs all three variables into a single byte. It is highly memory efficient, but accessing individual bit-fields requires the compiler to emit bitwise `AND` and `SHIFT` instructions, which slightly increases CPU instruction overhead.

---

**Q5. Why is `std::vector` almost always preferred over `std::list`?**

A: **Cache Locality and Hardware Prefetching.**
- `std::vector` stores elements contiguously in memory. When the CPU fetches an element from RAM, it pulls in an entire cache line (~64 bytes). The next several vector elements are already in the ultra-fast L1 cache. Furthermore, the CPU's **hardware prefetcher** detects the linear memory access pattern and preemptively loads upcoming data into the cache before you even ask for it.
- `std::list` is a doubly-linked list. Each node is dynamically allocated at arbitrary memory locations. Traversing a list results in constant "cache misses," forcing the CPU to wait hundreds of cycles for data to be fetched from extremely slow RAM. Pointer chasing destroys performance.

---

## Gotcha Code

```cpp
// Q: What does this print? (Assume 64-bit architecture)
struct Empty {};
std::cout << sizeof(Empty);

// A: It prints 1 (not 0).
// In C++, the size of an empty struct/class must be at least 1 byte. 
// This guarantees that every distinct object has a unique memory address.
// If it were 0, an array of Empty objects would all share the exact same pointer address,
// which breaks pointer arithmetic and identity.
```

---

## Coding Questions

**C1. Optimize the Struct**

Given the following struct representing an order in a trading system, optimize its memory layout to be as small as possible.

```cpp
struct Order {
    bool isBuy;
    double price;
    int orderId;
    char currency[3];
    long timestamp;
};
```

A:
Sort the members from largest alignment requirement to smallest:

```cpp
struct OptimizedOrder {
    double price;        // 8 bytes
    long timestamp;      // 8 bytes
    int orderId;         // 4 bytes
    char currency[3];    // 3 bytes
    bool isBuy;          // 1 byte
};                       // Total = 24 bytes (no internal padding, no tail padding)
```
Original size: `isBuy` (1) + 7 pad + `price` (8) + `orderId` (4) + `currency` (3) + 1 pad + `timestamp` (8) = **32 bytes**.
We saved 8 bytes per order (25% reduction in size)!
