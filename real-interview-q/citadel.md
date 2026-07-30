# Citadel / Citadel Securities — C++ Interview Questions

> **Focus:** Low-latency, lock-free, hardware sympathy

---

## Company-Specific Focus Questions

> See also: [citadel-prep.md](../citadel-prep.md) for full implementations and system design

**Lock-Free & Atomics**
1. Implement a lock-free SPSC (single-producer, single-consumer) queue **[Critical]**
2. Explain `memory_order_acquire` vs `memory_order_release` — when is each used? **[Critical]**
3. What is the cost of `seq_cst` vs `acquire/release` on x86? On ARM? **[Critical]**
4. Implement a spinlock using `std::atomic` — what memory ordering do you use?
5. What is the ABA problem? How do tagged pointers solve it?
6. What is false sharing? How do you detect and fix it? (`alignas(64)`, perf stat)
7. When would you use `compare_exchange_weak` vs `compare_exchange_strong`?

**Memory & Allocators**
8. Implement an arena (bump) allocator — explain the trade-offs
9. Implement a fixed-size object pool using a free list
10. What happens when you call `new`? Trace from C++ → malloc → kernel
11. What is placement new? When do you need explicit destructor calls?
12. Why are allocations non-deterministic? What is `mmap` vs `brk`?

**Performance & Cache**
13. Explain the CPU cache hierarchy (L1/L2/L3). What are the latency numbers?
14. What is cache-line size? How does it affect data structure design?
15. SoA vs AoS — when does each perform better?
16. What is branch prediction? How do you write branchless code?
17. Why is `std::vector` faster than `std::list` for traversal despite O(n) insert?
18. What is `[[likely]]` / `[[unlikely]]` (C++20)?

**System Design**
19. Design an in-memory limit order book with price-time priority
20. Design an SPSC market data pipeline — from network to strategy
21. How would you handle 10 million messages/second with < 1µs latency?

**Templates & Compile-Time**
22. How do you use CRTP to avoid virtual function overhead?
23. Write a compile-time lookup table using `constexpr`
24. When is `consteval` (C++20) better than `constexpr`?


---

## Master Gotcha Table

| Gotcha | The Trap | The Fix |
|--------|----------|---------|
| Virtual dispatch in constructor | vtable points to base during construction | Never call virtual functions in ctor/dtor |
| Non-virtual destructor | Derived destructor not called through base ptr | Always make base dtor virtual |
| Object slicing | Derived copied into base by value | Use pointers/references for polymorphism |
| Dangling reference | Returning reference to local variable | Return by value or use heap |
| Iterator invalidation | Using iterator after container modification | Reassign: `it = container.erase(it)` |
| shared_ptr from raw pointer | Two control blocks for same object | Copy the shared_ptr, or use enable_shared_from_this |
| std::move doesn't move | It just casts to T&&; you still need move ctor | Ensure move operations are defined |
| Signed integer overflow | UB — compiler assumes it doesn't happen | Use unsigned, or check before operation |
| new[] with delete | Must use delete[] for arrays | Prefer std::vector or unique_ptr<T[]> |
| Lambda capture by ref | Reference may outlive captured variable | Capture by value, or ensure lifetime |
| const_cast on true const | Modifying truly const object is UB | Only const_cast away const if original is non-const |
| Missing noexcept on move | std::vector copies instead of moves during realloc | Mark move ctor/assignment as noexcept |
