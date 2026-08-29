# Concurrency & Multithreading — Deep Theory & Mechanics

An exhaustive, low-level guide to the C++ memory model, hardware cache coherency (MESI), atomic operations, memory orderings, synchronization primitives, condition variable mechanics, lock-free data structures, and thread pool architectures.

---

## 1. Hardware Cache Coherency & Memory Orderings

Modern multi-core CPUs use multi-level hardware caches (L1, L2, L3) and the **MESI (Modified, Exclusive, Shared, Invalid)** protocol to coordinate cache lines across cores.

```
+-----------------------------------+     +-----------------------------------+
| CORE 0                            |     | CORE 1                            |
|  - Out-of-Order Execution Engine  |     |  - Out-of-Order Execution Engine  |
|  - Store Buffer / Load Queue      |     |  - Store Buffer / Load Queue      |
|  - L1 Data Cache (32 KB, Private) |     |  - L1 Data Cache (32 KB, Private) |
+-----------------|-----------------+     +-----------------|-----------------+
                  |                                         |
                  v                                         v
+-----------------------------------------------------------------------------+
| SHARED L3 CACHE (16 MB - 64 MB) & HARDWARE COHERENCY BUS (MESI PROTOCOL)     |
+-----------------------------------------------------------------------------+
                                       |
                                       v
+-----------------------------------------------------------------------------+
| MAIN SYSTEM RAM (DRAM)                                                      |
+-----------------------------------------------------------------------------+
```

### 1.1 The C++ Memory Orderings Spectrum

C++ defines 6 atomic memory orderings controlling compiler instruction reordering and CPU hardware memory barriers:

```
WEAKEST (Maximum Performance) <----------------------------------> STRICTEST (Total Order)
std::memory_order_relaxed -> acquire / release / acq_rel -> std::memory_order_seq_cst
```

| Memory Order | Operations | Hardware / Compiler Guarantee |
|---|---|---|
| **`relaxed`** | Read / Write / RMW | Guarantees atomicity and modification order of that single variable ONLY. Zero synchronization with other variables! |
| **`acquire`** | Read (Load) | **No subsequent reads or writes can be reordered BEFORE this load**. Pairs with `release`. |
| **`release`** | Write (Store) | **No prior reads or writes can be reordered AFTER this store**. All prior writes become visible to the acquiring thread! |
| **`acq_rel`** | RMW (Fetch-Add, CAS)| Combines both `acquire` (for subsequent ops) and `release` (for prior ops). |
| **`seq_cst`** | Read / Write / RMW | Sequentially Consistent: Imposes a globally agreed-upon total order across all threads. Default in C++. |

---

### 1.2 Release-Acquire Synchronization Pattern

The fundamental building block for message passing, lock-free queues, and custom mutexes:

```cpp
std::atomic<bool> ready{false};
int payload = 0;

// Thread 1 (Producer):
void producer() {
    payload = 42;                             // 1. Write non-atomic data
    ready.store(true, std::memory_order_release); // 2. Release store (acts as barrier)
}

// Thread 2 (Consumer):
void consumer() {
    while (!ready.load(std::memory_order_acquire)) { // 3. Acquire load
        // Spin or yield
    }
    std::cout << payload << "\n";             // 4. GUARANTEED to see 42! No data race!
}
```

---

## 2. Compare-And-Swap (CAS) & The ABA Problem

### 2.1 `compare_exchange_weak` vs `compare_exchange_strong`

```cpp
bool compare_exchange_weak(T& expected, T desired, std::memory_order order);
bool compare_exchange_strong(T& expected, T desired, std::memory_order order);
```

- **`compare_exchange_weak`**: Can experience **Spurious Failures** (fails even if `*this == expected` due to CPU cache-line invalidation, interrupts, or LL/SC architectures like ARM/RISC-V). **Must be used inside a loop**. Faster on RISC CPUs.
- **`compare_exchange_strong`**: Guaranteed not to fail spuriously. Use when CAS is executed in a non-looping conditional path.

```cpp
// Atomic addition loop using compare_exchange_weak
std::atomic<int> val{0};
void add(int delta) {
    int expected = val.load(std::memory_order_relaxed);
    while (!val.compare_exchange_weak(expected, expected + delta, 
                                      std::memory_order_release, 
                                      std::memory_order_relaxed)) {
        // expected is automatically updated with current value on failure!
    }
}
```

---

### 2.2 The ABA Problem in Lock-Free Data Structures

```
Initial Stack:           [ Top -> Node A ] -> [ Node B ] -> [ Node C ]

Step 1 (Thread 1): Reads Top = A, Next = B. Paused before CAS.
Step 2 (Thread 2): Pops A. Pops B. Pushes back A (reusing A's memory!).
Current Stack:           [ Top -> Node A ] -> [ Node C ]

Step 3 (Thread 1 resumes): Executes CAS(Top, A, B).
SUCCESS! But B was already freed! Stack top is corrupted to dangling pointer B!
```

#### Mitigations for the ABA Problem:
1. **Tagged Pointers / Version Counting (Double-Word CAS)**: Pack a 64-bit pointer with a 64-bit integer version counter. On each modification, increment version counter (`AtomicStampedReference`).
2. **Hazard Pointers**: Threads register active node pointers in a global thread-safe table. Nodes are never freed while registered as active.
3. **Epoch-Based Reclamation (RCU - Read-Copy-Update)**: Deferred deallocation based on global epoch ticks.

---

## 3. Synchronization Primitives

### 3.1 Mutex Varieties & RAII Wrappers

- **`std::mutex`**: Standard non-recursive mutual exclusion lock.
- **`std::shared_mutex` (C++17)**: Reader-Writer Lock. Supports shared read locks (`lock_shared()`) and exclusive write locks (`lock()`).
- **`std::scoped_lock` (C++17)**: Automatically acquires multiple mutexes simultaneously without deadlock using a deadlock-avoidance algorithm (equivalent to `std::lock(m1, m2)`):
  ```cpp
  std::scoped_lock lock(account1.mtx, account2.mtx); // Deadlock-free!
  ```

---

### 3.2 Condition Variables & Spurious Wakeups

A condition variable allows threads to sleep until notified by another thread:

```cpp
std::mutex mtx;
std::condition_variable cv;
std::queue<Task> queue;

void worker() {
    std::unique_lock<std::mutex> lock(mtx);
    // MANDATORY: Always use a predicate loop to handle Spurious Wakeups!
    cv.wait(lock, [&] { return !queue.empty() || is_shutdown; });

    if (!queue.empty()) {
        auto task = std::move(queue.front());
        queue.pop();
        lock.unlock(); // Release lock before executing heavy task!
        task();
    }
}
```

---

## 4. Modern C++20 Concurrency Primitives

### 4.1 `std::jthread` & Cooperative Cancellation (`std::stop_token`)
- Automatically calls `request_stop()` and `join()` in its destructor.

```cpp
std::jthread worker([](std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        do_work();
    }
});
// Automatically requested to stop and joined when worker goes out of scope!
```

### 4.2 `std::latch` & `std::barrier`
- **`std::latch`**: Single-use downward countdown counter (`count_down()`, `wait()`).
- **`std::barrier`**: Reusable multi-phase synchronization barrier with a completion callback.

### 4.3 `std::counting_semaphore<N>`
- Controls concurrent access to a finite pool of resources (`acquire()`, `release()`).

---

## 5. False Sharing & Cache Alignment

When two independent variables accessed by different CPU cores reside on the **same 64-byte cache line**, writes by Core 0 invalidate Core 1's cache line, causing massive interconnect traffic and severe latency penalties:

```cpp
// BAD: False Sharing (Both atomics share the same 64-byte cache line)
struct BadStats {
    std::atomic<uint64_t> thread0_writes{0};
    std::atomic<uint64_t> thread1_writes{0};
};

// GOOD: Cache Isolation (alignas(64) separates variables into distinct cache lines)
struct alignas(64) GoodStats {
    alignas(64) std::atomic<uint64_t> thread0_writes{0};
    alignas(64) std::atomic<uint64_t> thread1_writes{0};
};
```
