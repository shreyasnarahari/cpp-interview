# Low-Level Memory Layout & Optimization — Medium Questions

---

**Q1. What is False Sharing? How do you fix it using modern C++?**

A: CPU caches operate on **cache lines** (typically 64 bytes). If two threads on different cores frequently modify two independent variables that happen to reside on the *same* cache line, the CPU cores will constantly invalidate each other's cache copies (cache coherency traffic via MESI protocol). This forces expensive RAM fetches and destroys performance, even though there is no logical data race.

**Fix**: Ensure independent variables modified by different threads are placed on different cache lines. In C++17, you should use `std::hardware_destructive_interference_size`:

```cpp
#include <new>

struct alignas(std::hardware_destructive_interference_size) PaddedCounter {
    std::atomic<int> count{0};
};
PaddedCounter counters[2]; // Thread 1 uses counters[0], Thread 2 uses counters[1]
// They are guaranteed to be on separate cache lines.
```

---

**Q2. Explain Array of Structs (AoS) vs. Struct of Arrays (SoA).**

A: These are memory layout strategies.
**AoS (Array of Structs)**: The standard Object-Oriented approach.
```cpp
struct Particle { float x, y, z, velocity, mass; };
std::vector<Particle> particles;
```
**SoA (Struct of Arrays)**: The Data-Oriented approach.
```cpp
struct Particles {
    std::vector<float> x, y, z;
    std::vector<float> velocity;
    std::vector<float> mass;
};
```

**Why SoA is faster for large data**: If you need to run a physics simulation that only updates the `x, y, z` coordinates, AoS forces the CPU to load `velocity` and `mass` into the cache line anyway, wasting bandwidth. With SoA, the CPU loads *only* the contiguous X arrays, utilizing 100% of the cache line for useful data. SoA also enables the compiler to use **SIMD** (Single Instruction, Multiple Data) vectorization instructions trivially.

---

**Q3. Why does processing a sorted array run faster than an unsorted array?**

A: **Branch Prediction.**
Modern CPUs use deep instruction pipelines. When the CPU encounters an `if` statement, it must guess which branch will be taken to keep the pipeline full. 
- If the data is sorted, the condition (e.g., `if (data[i] > 128)`) is highly predictable (a long streak of `false` followed by a long streak of `true`). The CPU's branch predictor achieves near 100% accuracy.
- If unsorted, the condition behaves like a coin flip. The CPU frequently guesses wrong, forcing a **pipeline flush**, which wastes 10-20 CPU cycles per miss.

In C++20, you can provide hints to the compiler regarding branch prediction using `[[likely]]` and `[[unlikely]]`:
```cpp
if (x == 0) [[unlikely]] {
    throw std::runtime_error("Div by zero");
}
```

---

**Q4. What does the `inline` keyword actually do? When does inlining hurt performance?**

A: Historically, `inline` was a hint to the compiler to substitute the function call with the function body itself, eliminating function call overhead (saving registers, jumping addresses). Today, modern compilers ignore the `inline` hint for optimization purposes and decide internally based on heuristics. The primary standard C++ use of `inline` today is to prevent **One Definition Rule (ODR)** violations when defining functions in headers (it tells the linker to fold multiple identical definitions into one).

**When inlining hurts**: 
If a large function is inlined in many places, it severely inflates the size of the binary. This causes **Instruction Cache (I-cache) misses**, meaning the CPU has to constantly fetch instructions from RAM rather than keeping the tight loop in the fast L1 instruction cache. It's known as "code bloat".

---

## Gotcha Code

```cpp
// Q: Why is std::vector<bool> considered broken/dangerous?
std::vector<bool> flags = {true, false, true};
bool* ptr = &flags[0]; // Error?

// A: std::vector<bool> is a specialized container. 
// It does NOT store booleans as 1-byte chars. It packs them as BITS to save memory.
// Because you cannot take the address of a bit in C++, operator[] returns a proxy object,
// not a reference to a bool. 
// This breaks generic template algorithms expecting standard references.
// Also, modifying bits concurrently in std::vector<bool> causes massive false sharing!
// Fix: Use std::vector<char> or std::vector<uint8_t> for normal boolean arrays.
```

---

## Coding Questions

**C1. Matrix Iteration Cache Thrashing**

```cpp
// Q: Why might this loop be terribly slow, and how do you fix it?
int matrix[1000][1000];
int sum = 0;
for (int col = 0; col < 1000; col++) {
    for (int row = 0; row < 1000; row++) {
        sum += matrix[row][col];
    }
}
```

A: **Cache Thrashing.**
C/C++ uses row-major order for 2D arrays. Contiguous memory is located along rows. By iterating over rows in the inner loop, we are jumping around memory in strides of 1000 integers (~4000 bytes). This means we are pulling a 64-byte cache line from RAM, reading exactly one integer from it, and throwing it away. We miss the cache on *every single iteration*.

**Fix**: Swap the loops. Iterate `row` in the outer loop, and `col` in the inner loop. The CPU will sequentially read every integer within the 64-byte cache line before requesting the next one from RAM.
