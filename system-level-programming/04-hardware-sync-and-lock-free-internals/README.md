# Module 04: Hardware Synchronization & Lock-Free Internals

An exhaustive, low-latency engineering guide to hardware cache coherence protocols (MESI/MOESI), CPU store buffers, invalidate queues, the C++20 memory model, atomic operations, lock-free algorithms, and wait-free ring buffer design.

---

## 1. Hardware Cache Coherence: The MESI & MOESI Protocols

In multi-core SMP systems, each CPU core maintains private L1/L2 caches. To maintain a coherent view of DRAM, hardware implements the MESI protocol over the inter-core interconnect bus.

### MESI Cache Line States

| State | Name | Description | Permissions |
|---|---|---|---|
| **M** | Modified | Line is dirty, modified in this core's cache, not in DRAM, and present in *no other* core's cache. | Read + Write |
| **E** | Exclusive | Line matches DRAM, present in *only this* core's cache. | Read + Write (transitions to M) |
| **S** | Shared | Line matches DRAM, may be present in multiple cores' caches. | Read-Only |
| **I** | Invalid | Line does not contain valid data. Reading requires a bus request. | None |

```
                +-------------------+
                |   I (Invalid)     |
                +-------------------+
                  /               \
       Read Miss /                 \ Local Write (Read-For-Ownership)
                v                   v
      +---------------+       +------------------+
      |  S (Shared)   |       |  M (Modified)    |
      +---------------+       +------------------+
          |                       ^        |
          | Local Write           |        | Remote Read (Snoop)
          v                       |        v
      +---------------+-----------+   +---------------+
      | E (Exclusive) |               |  S (Shared)   |
      +---------------+               +---------------+
```

### Store Buffers & Invalidate Queues (Why Hardware Reorders Memory)

To avoid stalling CPU execution while waiting for an Invalidate Acknowledge bus transaction from remote cores:
1. **Store Buffer**: Writes are placed into a FIFO store buffer immediately, allowing the CPU to continue executing subsequent instructions. The store buffer drains asynchronously to the cache line when ownership is acquired.
2. **Invalidate Queue**: Invalidation requests from other cores are queued rather than immediately processed.
3. **The Consequence**: A subsequent load can execute *before* an earlier store has become visible to other cores (**StoreLoad Reordering**).

---

## 2. The C++20 Memory Model & Atomic Orderings

The C++ memory model standardizes how memory reads and writes synchronize across threads without raw platform assembly.

```
Weakest (Highest Performance) -----------------------------------> Strongest (Lowest Performance)
std::memory_order_relaxed   std::memory_order_acquire   std::memory_order_acq_rel   std::memory_order_seq_cst
                            std::memory_order_release
```

### 1. `std::memory_order_relaxed`
- Guarantees **atomicity** (no torn reads/writes) and **modification order consistency** on that single atomic variable.
- Enforces **NO synchronization or ordering** with other memory locations. The compiler and CPU can freely reorder surrounding reads and writes across this operation.

### 2. `std::memory_order_release` (Publishing / Producing)
- Used on store operations (`atomic.store(val, std::memory_order_release)`).
- **Guarantee**: All preceding memory writes (both atomic and non-atomic) in the current thread are guaranteed to be visible to any thread that loads this atomic variable with `memory_order_acquire`.
- **Fence Equivalent**: Prevents prior writes from being reordered *after* this store (`StoreStore` + `LoadStore` barrier).

### 3. `std::memory_order_acquire` (Consuming / Observing)
- Used on load operations (`atomic.load(std::memory_order_acquire)`).
- **Guarantee**: No subsequent memory reads or writes in the current thread can be reordered *before* this load.
- **Fence Equivalent**: Prevents subsequent reads/writes from being reordered *before* this load (`LoadLoad` + `LoadStore` barrier).

### 4. `std::memory_order_seq_cst` (Sequential Consistency)
- Default for all `std::atomic` operations.
- Enforces a single global total order of all `seq_cst` operations visible to all threads.
- Incurs heavy hardware bus synchronization (`mfence` or `lock xadd` / `xchg` instructions).

---

## 3. Compare-and-Swap (CAS) & ABA Prevention

Compare-and-Swap is the fundamental primitive for lock-free programming.

```cpp
bool compare_exchange_weak(T& expected, T desired, std::memory_order success, std::memory_order failure);
bool compare_exchange_strong(T& expected, T desired, std::memory_order success, std::memory_order failure);
```

- **`compare_exchange_weak`**: Allowed to fail spuriously (e.g. on LL/SC architectures like ARM or due to context switches on x86). Must always be used inside a loop (`while (!atomic.compare_exchange_weak(...))`). Faster than `strong` on many architectures.
- **`compare_exchange_strong`**: Guaranteed not to fail spuriously. Used when CAS is not in a retry loop.

### False Sharing Avoidance:
Placing two atomic variables accessed by different threads on the same 64-byte cache line causes catastrophic MESI bus bouncing ("False Sharing"). Always isolate independent concurrent variables:
```cpp
alignas(64) std::atomic<uint64_t> producer_tail_{0};
alignas(64) std::atomic<uint64_t> consumer_head_{0};
```

---

## 4. Lock-Free & Wait-Free Data Structures

### A. Wait-Free Single-Producer Single-Consumer (SPSC) Ring Buffer
- A bounded circular queue backed by a pre-allocated array of size $N$ ($N = 2^k$).
- Producer writes data, then updates `tail_` with `memory_order_release`.
- Consumer checks `tail_` with `memory_order_acquire`, reads data, then updates `head_` with `memory_order_release`.
- **Wait-Free Guarantee**: Every push and pop operation completes in a finite, deterministic number of steps ($O(1)$) with zero retries or loops!

### B. Bounded Multi-Producer Multi-Consumer (MPMC) Queue
- Dmitry Vyukov's algorithm: each array slot contains the payload and an atomic `sequence` counter.
- **Push**: Producer atomically claims a ticket (`tail_.fetch_add(1)`), waits until `cell.sequence == pos`, writes payload, and sets `cell.sequence = pos + 1` with `release`.
- **Pop**: Consumer claims a ticket (`head_.fetch_add(1)`), waits until `cell.sequence == pos + 1`, reads payload, and sets `cell.sequence = pos + Capacity` with `release`.

---

## 💻 Lab Exercises & Projects in this Module

1. **`exercises/memory_order_drills.hpp` & `tests/test_memory_orders.cpp`**:
   - Coding drills proving instruction reordering hazards with `relaxed` memory order and implementing the correct `acquire`-`release` message passing idiom.
2. **`projects/lockfree_ring_buffers/`**:
   - Production **Wait-Free SPSC Queue** and **Lock-Free MPMC Queue** in modern C++20.
   - Comprehensive multi-threaded stress tests (million-message verification under ASan & TSan).
   - Nanosecond throughput and latency benchmarks.
