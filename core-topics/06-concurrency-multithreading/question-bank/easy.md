# Concurrency — Easy Questions

---

**Q1. How do you create a thread?**

A:
```cpp
void work(int id) { std::cout << "Thread " << id << "\n"; }

std::thread t(work, 42);   // starts immediately
t.join();                   // wait for thread to finish

// With lambda:
std::thread t2([](){ std::cout << "Lambda thread\n"; });
t2.join();
```

If you don't `join()` or `detach()` before the `std::thread` destructor runs → `std::terminate()`.

---

**Q2. What is a mutex?**

A: Mutual exclusion — only one thread can hold the lock at a time:

```cpp
std::mutex mtx;
int counter = 0;

void increment() {
    std::lock_guard<std::mutex> lock(mtx);  // RAII lock
    counter++;
    // mutex automatically unlocked when lock goes out of scope
}
```

---

**Q3. What is `std::lock_guard` vs `std::unique_lock`?**

A:
| | `lock_guard` | `unique_lock` |
|---|---|---|
| Locking | Locks in constructor only | Can defer, try, time-out |
| Unlocking | Only on destruction | Can unlock/relock manually |
| Movable | No | Yes |
| Use with condition_variable | No | Yes (required) |
| Overhead | Minimal | Slightly more |

```cpp
// lock_guard — simple scoped locking
std::lock_guard<std::mutex> lock(mtx);

// unique_lock — flexible
std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
lock.lock();    // lock manually
lock.unlock();  // unlock manually
```

---

**Q4. What is `std::scoped_lock` (C++17)?**

A: Locks multiple mutexes simultaneously without deadlock:

```cpp
std::mutex m1, m2;

void safe() {
    std::scoped_lock lock(m1, m2);  // locks both, deadlock-free
    // work with resources protected by m1 and m2
}
```

Uses the same deadlock avoidance algorithm as `std::lock()`.

---

**Q5. What is `std::atomic`?**

A: Lock-free atomic operations on simple types:

```cpp
std::atomic<int> counter{0};

void increment() {
    counter++;                  // atomic increment
    counter.fetch_add(1);       // explicit atomic add
    counter.store(42);          // atomic store
    int val = counter.load();   // atomic load
}
```

No mutex needed. Works for integral types, pointers, and trivially copyable types.

---

## Coding Questions

---

**C1. Thread-safe counter**

A:
```cpp
class Counter {
    std::atomic<int> count_{0};
public:
    void increment() { count_++; }
    void decrement() { count_--; }
    int get() const { return count_.load(); }
};
```

---

**C2. Join all threads**

A:
```cpp
void parallelWork(int numThreads) {
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([i]() {
            std::cout << "Thread " << i << " working\n";
        });
    }
    for (auto& t : threads) {
        t.join();
    }
}
```

---

**C3. Fix the data race**

```cpp
// Buggy:
int counter = 0;
void work() { for (int i = 0; i < 10000; i++) counter++; }
// Two threads → result < 20000 (data race)

// Fixed with atomic:
std::atomic<int> counter{0};
void work() { for (int i = 0; i < 10000; i++) counter++; }
// Two threads → result == 20000 (guaranteed)
```

---

**C4. Meyer's Singleton (thread-safe)**

A:
```cpp
class Singleton {
public:
    static Singleton& instance() {
        static Singleton inst;  // thread-safe since C++11
        return inst;
    }
    void doWork() { /* ... */ }

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
private:
    Singleton() = default;
};
```
