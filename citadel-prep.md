# 🏦 Citadel / Citadel Securities — C++ Interview Prep

> **Target role:** C++ Software Engineer / Quant Developer
> **What makes Citadel different:** They care about *production-grade low-latency engineering*, not textbook answers. Every question has a follow-up: "How would you make this faster?" or "What happens under load?"

---

## Interview Format

| Stage | Format | Duration | Focus |
|---|---|---|---|
| **Online Assessment** | HackerRank/CoderPad | 60–90 min | DSA + probability/math |
| **Phone Screen (1–2)** | CoderPad live | 45–60 min each | C++ deep dive + coding |
| **Superday (3–6 rounds)** | On-site or virtual | 45–60 min each | Coding, C++ grill, system design, behavioral |

---

## Topic Priority — Reordered for Citadel

| Priority | Topic | Why |
|---|---|---|
| 🔴 **Critical** | Memory model, atomics, lock-free structures | Core of every Citadel C++ screen |
| 🔴 **Critical** | Cache optimization, false sharing, memory layout | Hardware sympathy is non-negotiable |
| 🔴 **Critical** | Move semantics, RAII, smart pointers | Ownership & lifetime — tested in every round |
| 🟡 **High** | Custom allocators, memory pools, placement new | "How do you avoid heap allocation on the hot path?" |
| 🟡 **High** | Templates, CRTP, constexpr, compile-time computation | "Move work from runtime to compile-time" |
| 🟡 **High** | STL internals — vector growth, map vs unordered_map, iterator invalidation | Tested through coding problems |
| 🟢 **Know well** | OOP, virtual dispatch, vtable layout | Usually one round of OOP design |
| 🟢 **Know well** | Exception safety, noexcept, RAII cleanup | Follow-up questions on error handling |
| 🟢 **Know well** | UB awareness — strict aliasing, signed overflow, sequence points | "What's wrong with this code?" |

---

## Section 1: Lock-Free SPSC Queue (Citadel's Signature Question)

### Why Citadel asks this
An SPSC (Single-Producer, Single-Consumer) queue is the backbone of low-latency trading systems — market data in → strategy decisions out. Citadel uses this to test your understanding of:
- Atomic operations and memory ordering
- False sharing and cache-line alignment
- Why lock-free > mutex for deterministic latency

### Implementation

```cpp
#include <atomic>
#include <array>
#include <optional>
#include <new>  // std::hardware_destructive_interference_size

template <typename T, size_t N>
class SPSCQueue {
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");  // for fast modulo

    // Each index on its own cache line to prevent false sharing
    alignas(64) std::atomic<size_t> head_{0};  // consumer reads/writes
    alignas(64) std::atomic<size_t> tail_{0};  // producer reads/writes
    alignas(64) std::array<T, N> buffer_;

    static constexpr size_t mask_ = N - 1;  // replaces % N with & mask_

public:
    // Producer thread only
    bool push(const T& value) {
        const size_t tail = tail_.load(std::memory_order_relaxed);  // only producer writes tail
        const size_t next = (tail + 1) & mask_;

        if (next == head_.load(std::memory_order_acquire)) {  // acquire: see consumer's writes
            return false;  // full
        }

        buffer_[tail] = value;
        tail_.store(next, std::memory_order_release);  // release: make buffer write visible
        return true;
    }

    // Consumer thread only
    std::optional<T> pop() {
        const size_t head = head_.load(std::memory_order_relaxed);  // only consumer writes head

        if (head == tail_.load(std::memory_order_acquire)) {  // acquire: see producer's writes
            return std::nullopt;  // empty
        }

        T value = buffer_[head];
        head_.store((head + 1) & mask_, std::memory_order_release);  // release: make read visible
        return value;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

    size_t size() const {
        return (tail_.load(std::memory_order_acquire) -
                head_.load(std::memory_order_acquire)) & mask_;
    }
};
```

### Key points the interviewer expects you to explain:

1. **Why `alignas(64)` on head and tail?**
   - Without it, head and tail may share a cache line (64 bytes on x86). When the producer writes tail, the consumer's cache line containing head is invalidated → **false sharing**.
   - `alignas(64)` puts them on separate cache lines → no cross-core coherency traffic.

2. **Why power-of-2 size?**
   - `(index + 1) % N` requires expensive division. `(index + 1) & (N-1)` is a single AND instruction.

3. **Memory ordering explained:**
   - `relaxed`: only the owner thread reads/writes this variable, no synchronization needed.
   - `release` (on store): "flush" — all prior writes (to buffer_) become visible to threads that `acquire` this atomic.
   - `acquire` (on load): "sync" — subsequent reads (from buffer_) see all writes before the corresponding `release`.
   - Together, acquire/release establish a **happens-before** relationship between producer's buffer write and consumer's buffer read.

4. **Why not `seq_cst` (default)?**
   - `seq_cst` adds a full memory barrier (MFENCE on x86) — ~20-50 cycles overhead per operation. In an SPSC queue, `acquire/release` provides sufficient ordering at near-zero cost on x86 (just prevents compiler reordering).

5. **Why no CAS/compare_exchange?**
   - SPSC doesn't need CAS because there's only one writer per variable. CAS loops are for MPMC (multiple-producer, multiple-consumer).

---

## Section 2: Memory Ordering Deep Dive

### The four orderings you must know:

| Ordering | Cost (x86) | What it does | When to use |
|---|---|---|---|
| `relaxed` | ~0 | No ordering guarantees between operations | Counters, statistics, flags only read by same thread |
| `acquire` | ~0 on x86 | Prevents reads after it from being reordered before it | Consumer side of producer-consumer |
| `release` | ~0 on x86 | Prevents writes before it from being reordered after it | Producer side of producer-consumer |
| `seq_cst` | ~20-50 cycles | Full fence — total global ordering | Default, when in doubt, or complex multi-variable protocols |

### x86 vs ARM:
- **x86** is strongly ordered — `acquire`/`release` are essentially free (compiler barrier only). `seq_cst` adds MFENCE.
- **ARM** is weakly ordered — `acquire`/`release` emit actual barrier instructions. Much bigger difference.

### "What happens if I use the wrong ordering?"

```cpp
// BUG: data race — producer's buffer writes may not be visible to consumer
std::atomic<size_t> tail{0};
buffer[tail] = value;
tail.store(next, std::memory_order_relaxed);  // no release!

// Consumer may see tail incremented BEFORE buffer is written → reads garbage
auto val = buffer[head];
head.store(next, std::memory_order_relaxed);  // no release!
```

### Common Citadel follow-up: "Implement a simple spinlock using atomics"

```cpp
class SpinLock {
    std::atomic<bool> locked_{false};
public:
    void lock() {
        while (locked_.exchange(true, std::memory_order_acquire)) {
            // spin — optionally add _mm_pause() / std::this_thread::yield()
        }
    }
    void unlock() {
        locked_.store(false, std::memory_order_release);
    }
};
```

---

## Section 3: Custom Memory Allocator (Arena/Pool)

### Why Citadel asks this
`new`/`delete` call `malloc`/`free` which may syscall (`brk`/`mmap`) — **non-deterministic latency**. On the hot path (microsecond-level trading), you pre-allocate memory and dispense it yourself.

### Arena Allocator (bump allocator)

```cpp
class Arena {
    char* buffer_;
    size_t capacity_;
    size_t offset_ = 0;
public:
    Arena(size_t size) : buffer_(static_cast<char*>(std::aligned_alloc(64, size))),
                         capacity_(size) {}
    ~Arena() { std::free(buffer_); }

    // Allocate — O(1), no syscall, no lock
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        size_t aligned_offset = (offset_ + alignment - 1) & ~(alignment - 1);
        if (aligned_offset + size > capacity_) throw std::bad_alloc();
        void* ptr = buffer_ + aligned_offset;
        offset_ = aligned_offset + size;
        return ptr;
    }

    // Reset — free everything at once (no per-object free)
    void reset() { offset_ = 0; }

    // Cannot free individual allocations — that's the trade-off
    // Perfect for: per-frame game allocations, per-message processing
};

// Usage:
Arena arena(1024 * 1024);  // 1 MB pre-allocated
auto* order = new (arena.allocate(sizeof(Order))) Order{...};
// At end of cycle:
arena.reset();  // O(1) — no destructors, no free list
```

### Object Pool (fixed-size)

```cpp
template <typename T, size_t N>
class ObjectPool {
    union Slot {
        T object;
        Slot* next;
        Slot() {}   // don't construct T
        ~Slot() {}  // don't destroy T
    };

    std::array<Slot, N> pool_;
    Slot* freeList_;
public:
    ObjectPool() {
        // Chain all slots into free list
        for (size_t i = 0; i < N - 1; i++)
            pool_[i].next = &pool_[i + 1];
        pool_[N - 1].next = nullptr;
        freeList_ = &pool_[0];
    }

    T* allocate() {
        if (!freeList_) return nullptr;
        Slot* slot = freeList_;
        freeList_ = slot->next;
        return new (&slot->object) T{};  // placement new
    }

    void deallocate(T* ptr) {
        ptr->~T();  // explicit destructor
        auto* slot = reinterpret_cast<Slot*>(ptr);
        slot->next = freeList_;
        freeList_ = slot;
    }
};

// Usage:
ObjectPool<Order, 10000> pool;
Order* o = pool.allocate();
pool.deallocate(o);
```

### Key points for the interviewer:
1. **Arena**: O(1) alloc, O(1) mass-free, no per-object free — perfect for per-message/per-tick processing.
2. **Pool**: O(1) alloc, O(1) free, fixed-size objects — perfect for Order, Trade, Quote objects.
3. Both avoid `malloc` → no syscalls → **deterministic latency**.
4. Know `placement new` and explicit destructor calls.

---

## Section 4: Order Book Design (System Design Round)

### The question
"Design an in-memory limit order book for a single instrument (e.g., AAPL). It receives order insert/cancel/modify messages and must maintain price-time priority. What data structures do you use?"

### Data Model

```cpp
struct Order {
    uint64_t orderId;
    double price;
    int quantity;
    bool isBuy;   // true = bid, false = ask
    uint64_t timestamp;
};

struct PriceLevel {
    double price;
    int totalQuantity;
    std::list<Order> orders;  // FIFO queue — price-time priority
};
```

### Data Structures — The Key Decision

```
Best Bid    Best Ask
  ↓            ↓
[102.50]    [102.51]
[102.49]    [102.52]
[102.48]    [102.53]
  ...         ...
```

| Structure | Lookup | Insert | Remove | Ordered |
|---|---|---|---|---|
| `std::map<double, PriceLevel>` | O(log n) | O(log n) | O(log n) | ✅ Yes |
| `std::unordered_map<double, PriceLevel>` | O(1) avg | O(1) avg | O(1) avg | ❌ No |

**Answer:** Use `std::map` for price levels (need ordered traversal for best bid/ask) + `std::unordered_map<uint64_t, iterator>` for O(1) order lookup by ID.

### Skeleton Implementation

```cpp
class OrderBook {
    // Price levels — ordered by price
    std::map<double, PriceLevel, std::greater<>> bids_;  // highest first
    std::map<double, PriceLevel, std::less<>> asks_;     // lowest first

    // O(1) order lookup by ID → points to exact position in price level
    using OrderIter = std::list<Order>::iterator;
    struct OrderLocation {
        double price;
        bool isBuy;
        OrderIter iter;
    };
    std::unordered_map<uint64_t, OrderLocation> orderIndex_;

public:
    void addOrder(Order order) {
        auto& book = order.isBuy ? bids_ : asks_;
        auto& level = book[order.price];
        level.price = order.price;
        level.totalQuantity += order.quantity;
        level.orders.push_back(order);

        orderIndex_[order.orderId] = {
            order.price, order.isBuy, std::prev(level.orders.end())
        };
    }

    void cancelOrder(uint64_t orderId) {
        auto it = orderIndex_.find(orderId);
        if (it == orderIndex_.end()) return;

        auto& loc = it->second;
        auto& book = loc.isBuy ? bids_ : asks_;
        auto levelIt = book.find(loc.price);
        if (levelIt == book.end()) return;

        levelIt->second.totalQuantity -= loc.iter->quantity;
        levelIt->second.orders.erase(loc.iter);

        if (levelIt->second.orders.empty())
            book.erase(levelIt);  // remove empty price level

        orderIndex_.erase(it);
    }

    std::optional<double> bestBid() const {
        if (bids_.empty()) return std::nullopt;
        return bids_.begin()->first;
    }

    std::optional<double> bestAsk() const {
        if (asks_.empty()) return std::nullopt;
        return asks_.begin()->first;
    }
};
```

### Citadel follow-ups to expect:
1. "How do you match orders?" (crossing the spread — when best bid >= best ask)
2. "How do you handle modify?" (cancel + re-add, or in-place if quantity decreases)
3. "What's the latency bottleneck?" (`std::map` → O(log n), consider flat sorted array for small number of price levels)
4. "How do you avoid allocations?" (pre-allocate Order objects in a pool, use intrusive list)
5. "What about multi-threaded access?" (one thread per book is common in HFT — avoids locks)

---

## Section 5: Hot-Path Optimization Patterns

### 1. Branch prediction — branchless code
```cpp
// Branchy (branch misprediction ~15 cycles):
int result = condition ? valueA : valueB;

// Branchless:
int results[2] = {valueB, valueA};
int result = results[condition];  // array lookup — no branch

// Or with arithmetic:
int result = valueB + condition * (valueA - valueB);
```

### 2. Cache-friendly iteration
```cpp
// BAD — pointer chasing (each node may be on a different cache line):
for (Node* n = head; n; n = n->next) process(n);  // linked list

// GOOD — sequential memory access (prefetcher loves this):
for (auto& item : vec) process(item);  // vector — contiguous memory
```

### 3. Avoid allocations on the hot path
```cpp
// BAD:
void process(const Data& d) {
    auto result = std::make_unique<Result>();  // heap allocation!
    // ...
}

// GOOD:
void process(const Data& d, Result& out) {
    // reuse pre-allocated Result
    out.reset();
    // ...
}
```

### 4. Compile-time dispatch (avoid virtual call overhead)
```cpp
// RUNTIME dispatch (~5ns indirect call + icache miss):
class Strategy { public: virtual double compute(double x) = 0; };

// COMPILE-TIME dispatch (inlined, zero overhead):
template <typename StrategyT>
class Engine {
    StrategyT strategy_;
public:
    double run(double x) { return strategy_.compute(x); }  // inlined
};
```

### 5. `[[likely]]` / `[[unlikely]]` (C++20)
```cpp
if (order.quantity > 0) [[likely]] {
    processOrder(order);
} else [[unlikely]] {
    handleError(order);
}
```

### 6. Struct of Arrays vs Array of Structs
```cpp
// AoS — poor cache utilization when only accessing prices:
struct Order { uint64_t id; double price; int qty; char side; };
std::vector<Order> orders;  // touching price touches all fields

// SoA — cache-friendly for column access:
struct OrderBook {
    std::vector<uint64_t> ids;
    std::vector<double> prices;   // contiguous — prefetcher happy
    std::vector<int> quantities;
    std::vector<char> sides;
};
```

---

## Section 6: Top 20 Citadel-Specific Q&A

### Memory & Ownership

**Q1. What is placement new? When would you use it?**
A: Constructs an object at a pre-allocated memory address without calling `malloc`. Used in memory pools, arena allocators, and containers. Must call destructor explicitly.
```cpp
char buf[sizeof(Widget)];
Widget* w = new (buf) Widget(42);  // construct in buf
w->~Widget();                       // explicit destruction
```

**Q2. Explain the control block of `std::shared_ptr`.**
A: A heap-allocated struct containing: strong ref count (atomic), weak ref count (atomic), deleter, allocator. `make_shared` allocates object + control block together (one allocation). `shared_ptr<T>(new T)` allocates separately (two allocations).

**Q3. What is the difference between `new`/`delete` and `malloc`/`free`?**
A: `new` calls `malloc` + constructor. `delete` calls destructor + `free`. `malloc`/`free` don't know about constructors/destructors. Mixing is UB.

### Concurrency & Atomics

**Q4. What is `std::memory_order_acquire` vs `std::memory_order_release`?**
A: `release` on a store: all prior writes become visible to any thread that does an `acquire` load on the same atomic. Together they create a happens-before edge without a full fence.

**Q5. What is false sharing?**
A: When two threads modify different variables that share a cache line (64 bytes). Each write invalidates the other core's cache → constant cross-core traffic. Fix: `alignas(64)` to separate them.

**Q6. Why is `seq_cst` slower than `acquire/release`?**
A: `seq_cst` establishes a single total order across all atomics — requires MFENCE on x86 (~20-50 cycles). `acquire/release` only orders operations on a single variable — free on x86 (compiler barrier only).

**Q7. Implement a lock-free stack using CAS.**
A:
```cpp
template <typename T>
class LockFreeStack {
    struct Node { T data; Node* next; };
    std::atomic<Node*> head_{nullptr};
public:
    void push(T value) {
        Node* node = new Node{std::move(value), nullptr};
        node->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(node->next, node,
                std::memory_order_release, std::memory_order_relaxed))
            ;  // CAS loop — retries if another thread modified head
    }
    // Note: pop has the ABA problem — production code needs hazard pointers
};
```

### Performance & Optimization

**Q8. What is cache locality and why does `std::vector` beat `std::list`?**
A: CPU caches load data in 64-byte cache lines. `vector` stores elements contiguously → sequential access hits the hardware prefetcher → ~100x faster traversal than `list` (random pointers → cache miss per node).

**Q9. What is the L1/L2/L3 cache hierarchy?**
A: L1: ~32-64 KB, ~1 ns. L2: ~256 KB-1 MB, ~3-5 ns. L3: ~8-32 MB, ~10-20 ns. Main memory: ~50-100 ns. Every cache miss is ~100x slower than an L1 hit.

**Q10. What is branch prediction? When does it fail?**
A: CPU predicts which branch will be taken and speculatively executes it. If wrong: ~15-20 cycle penalty (pipeline flush). Fails on: random/unpredictable conditions, tight loops with data-dependent branches. Fix: branchless code, sorting data to make branches predictable.

**Q11. SoA vs AoS — when does it matter?**
A: AoS (Array of Structs) is natural but wastes cache when you only access one field. SoA (Struct of Arrays) keeps each field contiguous — better for columnar access patterns (e.g., scanning all prices). HFT systems often use SoA.

### Templates & Compile-Time

**Q12. How do you move computation from runtime to compile-time?**
A: `constexpr` functions, `consteval` (C++20), template metaprogramming, `if constexpr` for compile-time branching, `constinit` for static init. Example: compile-time lookup tables, compile-time hash functions.

**Q13. What is CRTP and how does it avoid virtual function overhead?**
A: Base class takes derived as template parameter → calls derived methods via `static_cast` → compiler resolves at compile time → fully inlined, zero overhead (vs ~5ns for virtual call + possible icache miss).

### System Knowledge

**Q14. What happens when you call `new`?**
A: `operator new` → `malloc` → if no free block: `brk`/`mmap` syscall (kernel mode transition, ~1000 cycles). Then constructor is called. This is why HFT systems pre-allocate: syscalls are non-deterministic.

**Q15. What is a page fault?**
A: Virtual memory page not mapped to physical memory. Minor fault: page exists but not in TLB (~1 µs). Major fault: page must be loaded from disk (~10 ms). Pre-fault pages with `mmap` + `madvise(MADV_WILLNEED)` or `mlock` to avoid.

**Q16. What is `volatile` — and why is it NOT for multithreading?**
A: `volatile` prevents compiler optimization of reads/writes (for hardware registers). It does NOT provide atomicity, ordering, or synchronization. Use `std::atomic` for threads.

### UB & Correctness

**Q17. What is strict aliasing?**
A: Compiler assumes pointers of unrelated types don't alias. `int* → float*` dereference is UB. Exceptions: `char*`/`unsigned char*`/`std::byte*` can alias anything. Use `std::bit_cast` (C++20) for type punning.

**Q18. Why should destructors be `noexcept`?**
A: If a destructor throws during stack unwinding (another exception in flight), `std::terminate()` is called. Destructors are implicitly `noexcept` since C++11.

**Q19. What is the ABA problem in lock-free programming?**
A: Thread reads pointer A, gets preempted, another thread changes A→B→A. First thread's CAS succeeds (sees A) but the data has changed. Fix: tagged pointers (pack a counter with the pointer) or hazard pointers.

**Q20. What is `std::move_if_noexcept` and why does `vector` use it?**
A: Returns `T&&` if move is noexcept, otherwise returns `const T&` (forcing copy). `vector::push_back` uses this during reallocation — if move might throw, it copies to preserve the strong exception guarantee.

---

## Quick Reference: Citadel Red Flags (What NOT to Say)

| ❌ Don't say | ✅ Say instead |
|---|---|
| "Use `std::mutex` for everything" | "On the hot path, consider lock-free structures or single-threaded design" |
| "Just use `new`/`delete`" | "Pre-allocate with an arena or object pool to avoid malloc on the hot path" |
| "I'd use `std::list` for the queue" | "`std::vector` or a ring buffer — cache locality matters more than O(1) insert" |
| "`std::memory_order_seq_cst` is fine" | "For SPSC, `acquire/release` is sufficient and avoids the MFENCE penalty" |
| "Virtual functions are fine" | "On the hot path, use CRTP or templates — virtual calls have icache miss overhead" |
| "I haven't used sanitizers" | "I'd compile with `-fsanitize=address,undefined,thread` to catch bugs early" |
