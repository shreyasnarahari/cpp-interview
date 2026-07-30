# Theory & Mechanics: Concurrency & Multithreading

## 1. C++11 Memory Model & Thread Fundamentals

### Data Race vs Race Condition
- **Data Race**: Two or more concurrent threads access the same memory location without synchronization, where at least one access is a write. **A DATA RACE IS UNDEFINED BEHAVIOR (UB)**. The compiler is free to reorder, eliminate, or corrupt variables involved in data races.
- **Race Condition**: A high-level architectural flaw where the correctness of a program depends on the relative timing/order of execution. A program can be free of data races but still suffer from race conditions.

### Thread Lifecycle: `std::thread` vs `std::jthread` (C++20)
A `std::thread` object represents a system thread of execution:
- **Destructor Contract**: If a `std::thread` is still **joinable** (`joinable() == true`) when its destructor executes, `std::terminate()` is invoked immediately, crashing the process!
- **Fix**: You MUST call either `.join()` (wait for thread completion) or `.detach()` (decouple execution) before scope exit.
- **`std::jthread` (C++20)**: Automatically requests cancellation via `std::stop_token` and calls `.join()` inside its destructor, enforcing exception-safe thread management.

---

## 2. Mutexes & RAII Lock Wrappers

### Underlying Mutex Mechanics (Futex System Calls)
Modern Linux C++ mutexes (`std::mutex`) are implemented using **Futexes** (Fast Userspace Mutexes):
1. **Uncontended Case**: Uses an atomic Compare-And-Swap (`lock cmpxchg`) in userspace. Acquiring an unlocked mutex takes **~5 nanoseconds (zero syscalls)**.
2. **Contended Case**: If another thread holds the lock, the thread executes the `sys_futex` system call to suspend execution in the kernel and sleep until signaled.

```
Mutex Lock Comparison:

Wrapper             Movable?   Multiple Mutexes?   Use Case
─────────────────────────────────────────────────────────────────────────────
std::lock_guard     No         No                  Simple scoped mutex locking
std::unique_lock    Yes        No                  Condition variables, deferred locking
std::scoped_lock    No         Yes (Deadlock-Free) Locking 2+ mutexes safely (C++17)
std::shared_lock    Yes        No                  Reader lock for std::shared_mutex
```

### Preventing Deadlocks with `std::scoped_lock`
Deadlocks occur when two threads request locks in different order:
- **Thread A**: Locks `m1`, waits for `m2`.
- **Thread B**: Locks `m2`, waits for `m1`.

`std::scoped_lock` uses a deadlock avoidance algorithm (`std::lock` under the hood) that acquires multiple mutexes simultaneously without deadlock using a lock-ordering or back-off algorithm:

```cpp
std::mutex m1, m2;
void thread_safe_transfer() {
    // Locks BOTH m1 and m2 atomically without deadlock risk!
    std::scoped_lock lock(m1, m2); 
}
```

---

## 3. Condition Variables & Spurious Wakeups

`std::condition_variable` enables threads to sleep until a predicate condition changes.

### Why `std::unique_lock` is Required
`cv.wait(lock)` performs three atomic steps:
1. Atomically unlocks the mutex.
2. Suspends the calling thread and places it on the wait queue.
3. Upon notification, wakes up and re-acquires the mutex before returning.

### Spurious Wakeups
A thread sleeping on a condition variable may wake up **without any explicit signal** (due to OS kernel signal interrupts or multi-core wake scheduling).

**CRITICAL RULE**: Always pass a predicate lambda to `wait()`:
```cpp
std::mutex mtx;
std::condition_variable cv;
bool ready = false;

// Consumer Thread:
std::unique_lock<std::mutex> lock(mtx);
// Loop guards against spurious wakeups!
cv.wait(lock, [&]{ return ready; }); 
```

---

## 4. Atomics & Memory Orderings

`std::atomic<T>` provides lock-free or synchronized hardware atomic operations without mutexes.

### The 6 Memory Orderings:

```
                      Increasing Synchronization & Cost
  -------------------------------------------------------------------------->
  memory_order_relaxed   memory_order_acquire / release   memory_order_seq_cst
  (No ordering guarantees)   (Producer-Consumer sync)     (Total global order)
```

1. **`memory_order_relaxed`**: Guarantees atomicity for the variable itself, but NO ordering constraints relative to surrounding memory reads/writes.
2. **`memory_order_release`**: Used on **Store/Write** operations. Ensures all prior memory writes in the thread are committed before this store becomes visible.
3. **`memory_order_acquire`**: Used on **Load/Read** operations. Ensures subsequent memory reads in the thread cannot be reordered before this load.
4. **`memory_order_seq_cst`**: Default for all atomic operations. Implements **Sequential Consistency** (full memory fences across all CPU cores).

### Atomic Flag vs `std::atomic<bool>`
- `std::atomic_flag`: Guaranteed to be **100% lock-free** on all hardware architectures (`test_and_set()`).
- `std::atomic<bool>`: Usually lock-free, but may fall back to internal mutexes on specialized embedded architectures.

---

## 5. False Sharing & Hardware Cache Sympathy

When two threads running on different CPU cores continuously write to distinct variables located inside the **same 64-byte L1 cache line**, the CPU cache coherence protocol (MESI) forces constant cache line invalidations and bus traffic across cores.

```
False Sharing Scenario (Cache Line = 64 Bytes):
┌──────────────────────────────────────────────────────────────┐
│  Atomic Head (written by Core 0) │ Atomic Tail (written by Core 1) │
└──────────────────────────────────────────────────────────────┘
 ▲                                  ▲
 └─────────── SAME 64B CACHE LINE ──┘  ---> Constant Cache Line Invalidation!
```

### Prevention via Alignment:
```cpp
#include <new>

struct alignas(64) CacheAlignedCounter {
    std::atomic<uint64_t> value{0};
}; // Each counter occupies its own isolated 64-byte cache line!
```

---

## 6. Code Traps & Production Implementation Patterns

### Code Trap 1: Raw Mutex Early Return Leak
```cpp
std::mutex mtx;
void increment(int& counter) {
    mtx.lock();
    counter++;
    if (counter > 100) return; // BUG: Early return LEAKS mutex lock forever (Deadlock)!
    mtx.unlock();
}
// FIX: Use std::lock_guard<std::mutex> lock(mtx);
```

### Code Trap 2: Plain `bool` Spinlock (Data Race UB)
```cpp
bool ready = false;
void producer() { ready = true; }
void consumer() { while (!ready) {} } 
// BUG: Data race UB! Compiler may optimize while(!ready) into while(true) infinite loop!
// FIX: std::atomic<bool> ready{false};
```

### Thread-Safe Bounded Queue (Producer-Consumer Pattern)
```cpp
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue {
    std::queue<T> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_push_;
    std::condition_variable cv_pop_;
    size_t max_size_;
public:
    explicit ThreadSafeQueue(size_t max_size) : max_size_(max_size) {}

    void push(T item) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_push_.wait(lock, [&]{ return queue_.size() < max_size_; });
        queue_.push(std::move(item));
        cv_pop_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_pop_.wait(lock, [&]{ return !queue_.empty(); });
        T item = std::move(queue_.front());
        queue_.pop();
        cv_push_.notify_one();
        return item;
    }
};
```
