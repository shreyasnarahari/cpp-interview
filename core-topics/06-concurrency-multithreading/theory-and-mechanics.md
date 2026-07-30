# Theory & Mechanics: Concurrency & Multithreading

## 1. C++ Memory Model — Happens-Before

The C++11 memory model defines when one thread's writes are **guaranteed to be visible** to another thread's reads. Without synchronization, the compiler and CPU may reorder operations arbitrarily.

The **happens-before** relationship is established by:
- Operations within the same thread (sequenced-before)
- `mutex.lock()` / `mutex.unlock()` pairs
- Atomic operations with appropriate memory orderings
- Thread creation (`std::thread` constructor) and join

If A **happens-before** B, then B is guaranteed to see A's side effects.

## 2. Atomic Memory Orderings

```
Ordering Strength:
  relaxed  <  acquire/release  <  seq_cst

Cost on x86:
  relaxed     ~0 cycles  (compiler barrier only — x86 is strongly ordered)
  acq/rel     ~0 cycles  (compiler barrier only on x86)
  seq_cst     ~20-50 cycles  (emits MFENCE instruction)

Cost on ARM (weakly ordered):
  relaxed     ~0 cycles
  acq/rel     ~5-15 cycles  (emits DMB/barrier instructions)
  seq_cst     ~20-40 cycles (full barrier)
```

### `relaxed` — No ordering guarantees
```cpp
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);
// Only guarantees atomicity — not visibility to other threads
// Use for: statistics, counters where exact ordering doesn't matter
```

### `acquire` / `release` — Producer-consumer synchronization
```cpp
std::atomic<bool> ready{false};
int data = 0;

// Producer thread:
data = 42;                                          // (A)
ready.store(true, std::memory_order_release);       // (B) — release barrier
// Guarantee: (A) is visible to any thread that acquires (B)

// Consumer thread:
while (!ready.load(std::memory_order_acquire)) {}   // (C) — acquire barrier
assert(data == 42);                                 // (D) — guaranteed to see 42
// The acquire on (C) synchronizes-with the release on (B)
// Therefore: (A) happens-before (D)
```

### `seq_cst` — Total global ordering
All `seq_cst` operations across ALL threads appear in a single, globally agreed-upon order. This is the default and the most expensive — requires MFENCE on x86.

## 3. Mutex Internals — Futex

On Linux, `std::mutex` is typically implemented using **futex** (Fast Userspace muTEX):

```
Lock (optimistic path — no syscall):
  1. Atomic CAS: 0 → 1 (unlocked → locked)
  2. If CAS succeeds → mutex acquired (entirely in userspace, ~25 ns)

Lock (contended path — syscall):
  1. Atomic CAS fails (already locked by another thread)
  2. Spin briefly (adaptive spinning — check a few times)
  3. syscall: futex(FUTEX_WAIT) → kernel puts thread to sleep (~1-10 µs)

Unlock:
  1. Atomic store: 1 → 0 (locked → unlocked)
  2. If waiters exist: syscall futex(FUTEX_WAKE) to wake one sleeper
```

**Key insight:** An uncontended mutex lock/unlock is ~25 ns (no syscall). A contended lock involves kernel transitions (~µs). This is why lock-free structures matter for low-latency systems.

## 4. Condition Variable — Spurious Wakeups

A condition variable can wake up **without being notified** (spurious wakeup) — this is an artifact of the underlying OS implementation (signals, scheduler). You MUST use a predicate:

```cpp
std::mutex mtx;
std::condition_variable cv;
bool data_ready = false;

// WRONG — susceptible to spurious wakeups:
cv.wait(lock);  // may wake up even if no one called notify!

// CORRECT — predicate loop:
cv.wait(lock, [&]{ return data_ready; });
// Equivalent to:  while (!data_ready) { cv.wait(lock); }
```

## 5. False Sharing — Cache Line Contention

When two threads modify variables that **share a 64-byte cache line**, each write invalidates the other core's cached copy, causing constant MESI protocol traffic:

```
BAD — both counters share one cache line:
┌──────────────────────────────────────────────────────────────────┐
│ counter_a (4 bytes) │ counter_b (4 bytes) │ padding (56 bytes)  │ ← 64-byte cache line
└──────────────────────────────────────────────────────────────────┘
Thread 1 writes counter_a → invalidates Thread 2's cache line
Thread 2 writes counter_b → invalidates Thread 1's cache line
Result: constant cross-core cache coherency traffic (~100 cycles per write)

GOOD — separate cache lines with alignas(64):
┌──────────────────────────────────────────────────────────────────┐
│ counter_a (4 bytes) │ padding (60 bytes)                        │ ← cache line 1
└──────────────────────────────────────────────────────────────────┘
┌──────────────────────────────────────────────────────────────────┐
│ counter_b (4 bytes) │ padding (60 bytes)                        │ ← cache line 2
└──────────────────────────────────────────────────────────────────┘
No false sharing — each thread's writes stay on its own cache line.
```

```cpp
struct alignas(64) PaddedCounter {
    std::atomic<int> value{0};
};
PaddedCounter counters[2];  // guaranteed separate cache lines
```

## 6. `std::jthread` (C++20) — Auto-Joining + Cooperative Cancellation

```cpp
// std::thread requires manual join/detach — forgetting is UB:
std::thread t(work);
// If t is destroyed without join/detach → std::terminate()

// std::jthread auto-joins in destructor + supports stop tokens:
std::jthread jt([](std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        // do work...
    }
});
// Destructor: requests stop, then joins automatically
```
