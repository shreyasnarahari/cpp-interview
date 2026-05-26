# Topic 14: Low-Level Memory Layout & Optimization

## Why this topic matters

For roles in High-Frequency Trading (HFT), Quantitative Finance, Gaming (AAA engines), and Core Systems Infrastructure, writing "correct" C++ is not enough. You must write code that is sympathetic to the underlying hardware. 

Interviewers at firms like Jane Street, Citadel, Bloomberg, and Apple will grill you on how the compiler and the CPU interact with your C++ abstractions. They want to see if you understand that memory is not uniformly fast, that branches have a cost, and that object-oriented programming can sometimes be hostile to performance.

## Core Concepts to Master

1. **Struct Padding & Alignment**: Understanding why `sizeof(struct)` is rarely the sum of its parts, and how to pack data efficiently.
2. **Cache Locality**: Recognizing that RAM is slow and L1/L2/L3 caches are fast. Understanding the ~64-byte cache line.
3. **Data-Oriented Design**: Array of Structs (AoS) vs. Struct of Arrays (SoA).
4. **VTable Mechanics**: Understanding exactly how virtual dispatch works under the hood, where the `vptr` lives, and the true cost of polymorphism.
5. **Branch Prediction**: Understanding pipeline flushes and how to write branchless code.
6. **Compiler Optimizations**: Inlining, Devirtualization (via the `final` keyword), and Empty Base Optimization (EBO).

## 80/20 Rule for Interviews

If you only have time to master three things here:
1. **Struct Packing**: Be able to look at a `struct` definition and instantly calculate its `sizeof` on a 64-bit machine.
2. **Cache Lines & False Sharing**: Understand how two threads modifying independent variables can bottleneck each other if they share a cache line.
3. **VTable Layout**: Know exactly what happens in memory when an object inherits from a class with virtual functions.
