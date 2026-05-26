# Concurrency — Medium Questions

---

**Q1. What is a condition variable? What are spurious wakeups?**

A: A synchronization primitive that blocks a thread until a condition is met:

```cpp
std::mutex mtx;
std::condition_variable cv;
std::queue<int> q;

void producer() {
    std::lock_guard lock(mtx);
    q.push(42);
    cv.notify_one();  // wake one waiting consumer
}

void consumer() {
    std::unique_lock lock(mtx);
    cv.wait(lock, [&] { return !q.empty(); });  // predicate guards against spurious wakeups
    int val = q.front();
    q.pop();
}
```

**Spurious wakeups**: the OS may wake a thread even when no `notify` was called. Always use the predicate form of `wait()`.

---

**Q2. What is a deadlock? How do you prevent it?**

A: Two threads each hold a resource the other needs:

```
Thread A: holds m1, waits for m2
Thread B: holds m2, waits for m1
→ Neither can proceed
```

Prevention strategies:
1. **Lock ordering** — always acquire mutexes in the same order
2. **`std::scoped_lock`** — locks multiple mutexes atomically with deadlock avoidance
3. **Try-lock with backoff** — `try_lock()`, if fail, release all and retry
4. **Reduce scope** — hold locks for minimum time

---

**Q3. What is the difference between a data race and a race condition?**

A:
- **Data race**: Two threads access the same memory location, at least one writes, no synchronization. **Undefined behavior** in C++.
- **Race condition**: Program behavior depends on timing. Can exist even with proper synchronization (logic error).

```cpp
// Data race (UB):
int x = 0;
// Thread 1: x++
// Thread 2: x++
// Fix: std::atomic<int> x{0}

// Race condition (not UB, but logic bug):
std::mutex mtx;
// Thread 1: lock, check empty, unlock, lock, pop → might be empty now!
```

---

**Q4. When `std::atomic` vs `std::mutex`?**

A:
| Use `std::atomic` when | Use `std::mutex` when |
|---|---|
| Simple read/modify/write on single variable | Protecting multi-variable invariants |
| Counter, flag, pointer | Complex data structures |
| Lock-free performance critical | Operations that take time (I/O, computation) |

---

## Coding Questions

---

**C1. Thread-safe queue (producer-consumer)**

A:
```cpp
template <typename T>
class ThreadSafeQueue {
    std::queue<T> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
public:
    void push(T value) {
        {
            std::lock_guard lock(mtx_);
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    T pop() {
        std::unique_lock lock(mtx_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    bool try_pop(T& value) {
        std::lock_guard lock(mtx_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard lock(mtx_);
        return queue_.empty();
    }
};
```

---

**C2. Reader-writer lock**

A:
```cpp
class SharedData {
    std::shared_mutex mtx_;
    std::unordered_map<std::string, int> data_;
public:
    int read(const std::string& key) const {
        std::shared_lock lock(mtx_);  // multiple readers OK
        auto it = data_.find(key);
        return it != data_.end() ? it->second : -1;
    }

    void write(const std::string& key, int value) {
        std::unique_lock lock(mtx_);  // exclusive writer
        data_[key] = value;
    }
};
```

---

**C3. Parallel accumulate**

A:
```cpp
template <typename Iterator>
int parallelSum(Iterator begin, Iterator end, int numThreads) {
    auto len = std::distance(begin, end);
    auto chunkSize = len / numThreads;

    std::vector<std::future<int>> futures;
    auto chunkBegin = begin;

    for (int i = 0; i < numThreads; i++) {
        auto chunkEnd = (i == numThreads - 1) ? end : std::next(chunkBegin, chunkSize);
        futures.push_back(std::async(std::launch::async,
            [](Iterator b, Iterator e) {
                return std::accumulate(b, e, 0);
            }, chunkBegin, chunkEnd));
        chunkBegin = chunkEnd;
    }

    int total = 0;
    for (auto& f : futures) total += f.get();
    return total;
}
```

---

**C4. Once flag initialization**

A:
```cpp
class ExpensiveResource {
    static std::once_flag flag_;
    static std::unique_ptr<ExpensiveResource> instance_;
public:
    static ExpensiveResource& get() {
        std::call_once(flag_, []() {
            instance_ = std::make_unique<ExpensiveResource>();
        });
        return *instance_;
    }
};
```
