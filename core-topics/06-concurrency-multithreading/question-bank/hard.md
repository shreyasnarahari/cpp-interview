# Concurrency — Hard Questions

---

**Q1. Explain C++ memory orderings.**

A: `std::memory_order` controls how atomic operations synchronize memory between threads:

| Ordering | Guarantees |
|---|---|
| `relaxed` | Atomicity only — no ordering guarantees between threads |
| `acquire` | Reads after this see all writes before the corresponding release |
| `release` | Writes before this are visible to threads that acquire |
| `acq_rel` | Both acquire and release |
| `seq_cst` | Total order across all threads (default, strongest, slowest) |

```cpp
std::atomic<bool> ready{false};
int data = 0;

void producer() {
    data = 42;                                    // non-atomic write
    ready.store(true, std::memory_order_release);  // release: data visible to acquirer
}

void consumer() {
    while (!ready.load(std::memory_order_acquire)) {}  // acquire: sees data=42
    assert(data == 42);  // guaranteed!
}
```

Without acquire/release, `data = 42` might not be visible to the consumer even after `ready` is true.

---

**Q2. What is false sharing?**

A: When two threads access different variables that share the same cache line (~64 bytes), each thread's write invalidates the other's cache — causing unnecessary cache misses.

```cpp
// False sharing — both counters on same cache line
struct Counters {
    std::atomic<int> a{0};  // offset 0
    std::atomic<int> b{0};  // offset 4 — same cache line!
};

// Fix — pad to separate cache lines
struct alignas(64) PaddedCounter {
    std::atomic<int> value{0};
};
PaddedCounter counters[2];  // each on its own cache line
```

---

**Q3. What is a spinlock?**

A: A lock that busy-waits (spins) instead of putting the thread to sleep:

```cpp
class Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // spin — busy wait
        }
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }
};
```

Good for very short critical sections (< ~1μs). Bad for long critical sections (wastes CPU).

---

**Q4. Explain `std::promise` and `std::future`.**

A: One-shot channel for passing a value between threads:

```cpp
std::promise<int> promise;
std::future<int> future = promise.get_future();

std::thread producer([&promise]() {
    int result = computeExpensiveThing();
    promise.set_value(result);  // send value
});

int result = future.get();  // blocks until value is ready
producer.join();
```

---

## Coding Questions

---

**C1. Thread pool**

A:
```cpp
class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    ThreadPool(size_t numThreads) {
        for (size_t i = 0; i < numThreads; i++) {
            workers_.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mtx_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    template <typename F>
    void submit(F&& task) {
        {
            std::lock_guard lock(mtx_);
            tasks_.push(std::forward<F>(task));
        }
        cv_.notify_one();
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }
};

// Usage:
ThreadPool pool(4);
pool.submit([]() { std::cout << "Task 1\n"; });
pool.submit([]() { std::cout << "Task 2\n"; });
```

---

**C2. Lock-free stack (ABA-prone, simplified)**

A:
```cpp
template <typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
        Node(T val) : data(std::move(val)), next(nullptr) {}
    };
    std::atomic<Node*> head_{nullptr};

public:
    void push(T val) {
        auto* node = new Node(std::move(val));
        node->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(node->next, node,
            std::memory_order_release, std::memory_order_relaxed)) {}
    }

    std::optional<T> pop() {
        Node* old = head_.load(std::memory_order_relaxed);
        while (old && !head_.compare_exchange_weak(old, old->next,
            std::memory_order_acquire, std::memory_order_relaxed)) {}
        if (!old) return std::nullopt;
        T val = std::move(old->data);
        delete old;  // simplified — real code needs hazard pointers for ABA safety
        return val;
    }
};
```

Note: this has the ABA problem. Production code uses hazard pointers or epoch-based reclamation.

---

**C3. Barrier implementation**

A:
```cpp
class Barrier {
    std::mutex mtx_;
    std::condition_variable cv_;
    int count_;
    int threshold_;
    int generation_ = 0;

public:
    explicit Barrier(int count) : count_(count), threshold_(count) {}

    void wait() {
        std::unique_lock lock(mtx_);
        int gen = generation_;
        if (--count_ == 0) {
            generation_++;
            count_ = threshold_;
            cv_.notify_all();
        } else {
            cv_.wait(lock, [this, gen] { return gen != generation_; });
        }
    }
};
```
