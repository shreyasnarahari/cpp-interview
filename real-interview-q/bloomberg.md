# Bloomberg — C++ Interview Questions

> **Focus:** RAII, const correctness, OOP, Systems Design, Thread Safety, Low-Latency Financial Data Structures

---

## Questions by Topic

### Memory Management & RAII

**Beginner**
- What is RAII? Why is it important in C++? **[G, BB]**

**Intermediate**
- What is `std::unique_ptr`? How does it differ from `std::shared_ptr`? **[G, M, BB]**
- Why should you use `std::make_unique` / `std::make_shared` instead of `new`? *(exception safety, single allocation for shared_ptr)* **[G, BB]**
- What is the overhead of `std::shared_ptr` compared to raw pointers? *(control block + atomic refcount)* **[JT, BB]**

**Advanced**
- How does `std::make_shared` differ from `std::shared_ptr<T>(new T)` in terms of memory layout? *(single allocation vs two)* **[G, BB]**

**Gotcha Code**
```cpp
// Q: What's wrong with this code?
void process() {
    int* p = new int[100];
    doWork(p);   // might throw
    delete p;    // wrong delete AND leak if doWork throws
}
// A: Two bugs: (1) new[] must use delete[], not delete — UB.
// (2) If doWork() throws, delete is never reached → memory leak.
// Fix: use std::vector<int> or std::unique_ptr<int[]>.
```

```cpp
// Q: Is this safe?
std::shared_ptr<Widget> sp1(new Widget);
std::shared_ptr<Widget> sp2(sp1.get()); // ?
// A: NO — double delete! sp2 creates a separate control block for the
// same raw pointer. When both go out of scope, Widget is deleted twice → UB.
// Fix: copy the shared_ptr: auto sp2 = sp1;
```

```cpp
// Q: What does this print?
struct Node {
    std::shared_ptr<Node> next;
    ~Node() { std::cout << "destroyed\n"; }
};

void leak() {
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->next = b;
    b->next = a;  // circular reference
}
// A: Prints nothing — neither Node is ever destroyed.
// The circular shared_ptr references keep refcounts at 1 forever.
// Fix: make one of the pointers a std::weak_ptr.
```

### OOP & Polymorphism

**Beginner**
- Why must a base class destructor be virtual if used polymorphically? **[G, M, BB]**

**Intermediate**
- Explain the vtable mechanism. How does dynamic dispatch work? **[G, JT, BB]**
- What is the Rule of Three / Rule of Five / Rule of Zero? **[G, M, BB]**

**Advanced**
- How does `dynamic_cast` work internally? What does it need? *(RTTI, at least one virtual function)* **[G, BB]**

**Gotcha Code**
```cpp
// Q: What does this print?
class Base {
public:
    Base() { init(); }
    virtual void init() { std::cout << "Base::init\n"; }
};

class Derived : public Base {
public:
    void init() override { std::cout << "Derived::init\n"; }
};

int main() {
    Derived d;
}
// A: Prints "Base::init" — NOT "Derived::init".
// During Base construction, the vtable points to Base's methods.
// Virtual dispatch does NOT work in constructors/destructors.
```

```cpp
// Q: What's wrong here?
class Base {
public:
    ~Base() { std::cout << "Base destroyed\n"; }
};

class Derived : public Base {
    int* data;
public:
    Derived() : data(new int[100]) {}
    ~Derived() { delete[] data; }
};

void f() {
    Base* p = new Derived();
    delete p;  // ?
}
// A: Only ~Base() is called — ~Derived() never runs → memory leak.
// The destructor is not virtual, so delete through Base* calls ~Base().
// Fix: virtual ~Base() { ... }
```

```cpp
// Q: Does this compile?
class Animal {
public:
    virtual void speak() = 0;
};

class Dog : public Animal {
public:
    void speak() { std::cout << "Woof\n"; }
};

int main() {
    Animal a;  // ?
}
// A: Compile error — Animal is abstract (has pure virtual function).
// Cannot instantiate abstract classes. Fix: Animal* a = new Dog();
```

### Templates

**Intermediate**
- What is SFINAE? Give an example. **[G, JT, BB]**

**Gotcha Code**
```cpp
// Q: Does this compile? If so, what does it print?
template <typename T>
void print(T val) { std::cout << "generic: " << val << "\n"; }

template <>
void print<int>(int val) { std::cout << "int: " << val << "\n"; }

void print(int val) { std::cout << "non-template: " << val << "\n"; }

int main() {
    print(42);
    print<int>(42);
    print(3.14);
}
// A: "non-template: 42" — non-template wins over template for exact match
//    "int: 42" — explicit <int> forces template, specialization is picked
//    "generic: 3.14" — only the generic template matches double
```

```cpp
// Q: Why does this fail to compile?
template <typename T>
class Stack {
    std::vector<T> data;
public:
    void push(const T& val) { data.push_back(val); }
    T pop() {
        T top = data.back();
        data.pop_back();
        return top;
    }
};

template <typename T>
class Stack<T*> {  // Partial specialization
    // ...
};

template <>
void Stack<int>::push(const int& val) { /* ... */ } // Error?
// A: You cannot specialize a member function of a class template
// without specializing the entire class. You'd need to fully
// specialize Stack<int> first.
```

### Move Semantics & Value Categories

**Beginner**
- What is an lvalue? What is an rvalue? **[G, M, BB]**
- What does `std::move` do? *(it's just a cast to T&&, it doesn't move anything)* **[G, M, BB]**

**Intermediate**
- What is the state of a moved-from object? *(valid but unspecified)* **[G, BB]**
- What is copy elision? What is RVO vs NRVO? **[G, JT, BB]**
- Why should move operations be `noexcept`? *(std::vector only uses move if noexcept)* **[G, BB, JT]**

**Gotcha Code**
```cpp
// Q: What does this print?
class Widget {
public:
    Widget() { std::cout << "default\n"; }
    Widget(const Widget&) { std::cout << "copy\n"; }
    Widget(Widget&&) { std::cout << "move\n"; }
};

Widget createWidget() {
    Widget w;
    return w;  // NRVO candidate
}

int main() {
    Widget w = createWidget();
}
// A: Most likely "default" only (NRVO elides the move/copy).
// Without NRVO: "default" then "move". Copy is never used here.
```

```cpp
// Q: Is this correct?
void process(std::string&& s) {
    std::string local = s;  // copy or move?
}
// A: COPY! Even though s is declared as &&, inside the function s is
// an lvalue (it has a name). You must std::move(s) to trigger move.
// Fix: std::string local = std::move(s);
```

```cpp
// Q: What's wrong with this code?
std::unique_ptr<Widget> create() {
    auto w = std::make_unique<Widget>();
    return std::move(w);  // correct?
}
// A: It works, but std::move is HARMFUL here — it prevents NRVO.
// Fix: just return w; — the compiler will apply NRVO or implicit move.
```

### STL Containers & Iterators

**Beginner**
- What is `push_back` vs `emplace_back`? **[G, M, BB]**

**Intermediate**
- Explain iterator invalidation rules for `std::vector`. **[G, M, BB]**
- What happens to iterators when you insert into a `std::map`? *(they remain valid)* **[G, BB]**

**Advanced**
- What is the Small Buffer Optimization in `std::string`? **[JT, BB]**

**Gotcha Code**
```cpp
// Q: What's wrong with this code?
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it % 2 == 0) {
        v.erase(it);  // ?
    }
}
// A: Undefined behavior — erase invalidates the iterator `it`.
// After erase, `it` is dangling, and ++it is UB.
// Fix: it = v.erase(it); (erase returns next valid iterator)
// and don't ++it in the for when erasing.
```

```cpp
// Q: Is this safe?
std::vector<int> v = {1, 2, 3};
int& ref = v[0];
v.push_back(4);
std::cout << ref;  // ?
// A: Undefined behavior — push_back may trigger reallocation,
// which invalidates ALL references, pointers, and iterators.
// After reallocation, ref points to freed memory.
```

```cpp
// Q: What does this print?
std::map<int, std::string> m;
std::cout << m[42];  // ?
// A: Prints "" (empty string). operator[] inserts a default-constructed
// value if the key doesn't exist. m now has size 1 with key 42.
// This is why operator[] can't be used on const maps.
// Use m.at(42) to throw, or m.find(42) to check safely.
```

### Concurrency & Multithreading

**Beginner**
- What is a mutex? Why do you need it? **[G, M, BB]**

**Intermediate**
- What is the difference between `std::lock_guard` and `std::unique_lock`? **[G, BB]**
- What is a condition variable? What are spurious wakeups? **[G, M, BB]**
- What is a deadlock? How do you prevent it? **[G, M, AM, BB]**
- What is `std::atomic`? When would you use it instead of a mutex? **[G, JT, BB]**
- What is `std::scoped_lock` (C++17) and why is it preferred over multiple `lock_guard`? **[G, BB]**

**Advanced**
- Implement a thread-safe singleton. **[G, M, BB]**

**Gotcha Code**
```cpp
// Q: What's wrong with this code?
std::mutex mtx;
int counter = 0;

void increment() {
    mtx.lock();
    counter++;
    if (counter > 100) {
        return;  // BUG!
    }
    mtx.unlock();
}
// A: If counter > 100, the function returns without unlocking the mutex.
// Next call to lock() will deadlock (or throw with recursive_mutex).
// Fix: use std::lock_guard<std::mutex> lock(mtx); — RAII guarantees unlock.
```

```cpp
// Q: Is this thread-safe?
bool ready = false;

void producer() {
    // prepare data...
    ready = true;  // signal consumer
}

void consumer() {
    while (!ready) {}  // spin-wait
    // use data...
}
// A: NO — data race on `ready`. Both threads access a non-atomic bool
// without synchronization. The compiler may optimize away the loop
// (ready never changes from consumer's perspective).
// Fix: std::atomic<bool> ready{false};
```

```cpp
// Q: Does this avoid deadlock?
std::mutex m1, m2;

void threadA() {
    std::lock_guard<std::mutex> l1(m1);
    std::lock_guard<std::mutex> l2(m2);
    // work...
}

void threadB() {
    std::lock_guard<std::mutex> l1(m2);  // opposite order!
    std::lock_guard<std::mutex> l2(m1);
    // work...
}
// A: NO — classic deadlock. threadA holds m1, waits for m2.
// threadB holds m2, waits for m1. Circular wait.
// Fix: std::scoped_lock lock(m1, m2); — uses deadlock avoidance algorithm.
```

### Lambda Expressions

**Intermediate**
- What is a lambda capture? Explain [=], [&], [x], [&x], [this], [*this]. **[G, M, BB]**

**Advanced**
- What is an immediately invoked lambda expression (IILE)? When is it useful? **[G, BB]**
- What is the lifetime issue with lambda captures by reference? **[G, M, BB]**

**Gotcha Code**
```cpp
// Q: What does this print?
int x = 10;
auto fn = [x]() mutable {
    x += 5;
    std::cout << x << " ";
};
fn();
fn();
std::cout << x;
// A: "15 20 10" — the lambda captures x BY VALUE. The mutable keyword
// lets it modify its own copy. The original x is never changed.
// Each call modifies the lambda's internal copy, which persists between calls.
```

```cpp
// Q: What's the problem?
std::function<void()> createTimer() {
    std::string message = "timeout!";
    return [&message]() {
        std::cout << message;  // ?
    };
}
// A: Dangling reference — message is destroyed when createTimer returns.
// The lambda captures it by reference, so it's a dangling reference.
// Fix: capture by value [message] or [msg = std::move(message)]
```

```cpp
// Q: Does this compile?
auto fn = [](int a, int b) { return a + b; };
fn = [](int a, int b) { return a * b; };  // ?
// A: NO — each lambda has a unique type. You can't reassign a lambda
// variable to a different lambda. Fix: use std::function<int(int,int)>.
```

### Const Correctness & Type Casting

**Beginner**
- What is a const member function? **[G, BB]**

**Intermediate**
- What is the `mutable` keyword? Give a real use case. *(caching, mutex in const method)* **[G, BB, JT]**
- When would you use `const_cast`? Is it ever safe? *(only if the original object is non-const)* **[G, BB]**
- What is the difference between `constexpr` and `const`? **[G, M, BB]**

**Advanced**
- Why can't you call a non-const method on a const object? Explain the const overloading pattern. **[G, BB]**

**Gotcha Code**
```cpp
// Q: Does this compile?
const int ci = 42;
int* p = const_cast<int*>(&ci);
*p = 100;
std::cout << ci << " " << *p;
// A: Compiles, but *p = 100 is UNDEFINED BEHAVIOR because ci is
// truly const (defined as const). The compiler may have inlined 42
// everywhere ci appears. Output is unpredictable.
// const_cast is only safe when the original object is actually non-const.
```

```cpp
// Q: What is printed?
class Cache {
    mutable int hits = 0;
public:
    int get() const {
        hits++;  // OK — mutable member
        return 42;
    }
    int getHits() const { return hits; }
};

const Cache c;
c.get();
c.get();
std::cout << c.getHits();
// A: Prints 2. mutable allows modification even on const objects.
// Common use case: caching, logging, mutex locking in const methods.
```

---

## Bloomberg Deep-Dive & Real Interview Question Bank (52 Questions with Full Solutions)

---

### Category 1: Low-Latency & Real-Time Financial Data Systems

#### Q1. Design Underground System (`checkIn`, `checkOut`, `getAverageTime`) [Bloomberg High-Frequency]
**Question:** Design a system that tracks passenger travel times between stations and calculates average travel duration between any two stations. All operations must run in $O(1)$ average time.

```cpp
#include <string>
#include <unordered_map>
#include <utility>

class UndergroundSystem {
    // passengerId -> {startStation, checkInTime}
    std::unordered_map<int, std::pair<std::string, int>> check_ins_;
    
    // "startStation->endStation" -> {totalTime, totalPassengers}
    std::unordered_map<std::string, std::pair<double, int>> route_stats_;

public:
    UndergroundSystem() = default;

    void checkIn(int id, std::string stationName, int t) {
        check_ins_[id] = {std::move(stationName), t};
    }

    void checkOut(int id, std::string stationName, int t) {
        auto it = check_ins_.find(id);
        if (it == check_ins_.end()) return;

        const auto& [start_station, start_time] = it->second;
        std::string route = start_station + "->" + stationName;
        int duration = t - start_time;

        auto& [total_time, count] = route_stats_[route];
        total_time += duration;
        count += 1;

        check_ins_.erase(it); // Clean up active passenger
    }

    double getAverageTime(std::string startStation, std::string endStation) const {
        std::string route = startStation + "->" + endStation;
        auto it = route_stats_.find(route);
        if (it == route_stats_.end()) return 0.0;
        return it->second.first / it->second.second; // total_time / count
    }
};
```
- **Time Complexity:** $O(1)$ average for `checkIn`, `checkOut`, and `getAverageTime`.
- **Space Complexity:** $O(P + R)$ where $P$ is active passengers and $R$ is distinct routes.

---

#### Q2. Design Leaderboard (`addScore`, `top`, `reset`) [Bloomberg High-Frequency]
**Question:** Design a Leaderboard supporting `addScore(playerId, score)`, `top(K)` returning the sum of top K scores, and `reset(playerId)`.

```cpp
#include <unordered_map>
#include <set>
#include <numeric>

class Leaderboard {
    std::unordered_map<int, int> player_scores_; // playerId -> score
    std::multiset<int, std::greater<int>> sorted_scores_; // Highest to lowest

public:
    void addScore(int playerId, int score) {
        if (player_scores_.count(playerId)) {
            int old_score = player_scores_[playerId];
            sorted_scores_.erase(sorted_scores_.find(old_score));
            int new_score = old_score + score;
            player_scores_[playerId] = new_score;
            sorted_scores_.insert(new_score);
        } else {
            player_scores_[playerId] = score;
            sorted_scores_.insert(score);
        }
    }

    int top(int K) {
        int sum = 0;
        int count = 0;
        for (int score : sorted_scores_) {
            sum += score;
            if (++count == K) break;
        }
        return sum;
    }

    void reset(int playerId) {
        if (player_scores_.count(playerId)) {
            int old_score = player_scores_[playerId];
            sorted_scores_.erase(sorted_scores_.find(old_score));
            player_scores_.erase(playerId);
        }
    }
};
```
- **Time Complexity:** `addScore`: $O(\log N)$, `reset`: $O(\log N)$, `top(K)`: $O(K)$.

---

#### Q3. Design In-Memory Key-Value Store with TTL & Transactions [Bloomberg Core Infrastructure]
**Question:** Implement an in-memory key-value store supporting `set(key, val, ttl)`, `get(key)`, and nested transaction commands (`begin()`, `commit()`, `rollback()`).

```cpp
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

class TransactionalKV {
    using TimePoint = std::chrono::steady_clock::time_point;
    struct ValueEntry {
        std::string value;
        std::optional<TimePoint> expiry;
    };

    std::unordered_map<std::string, ValueEntry> db_;
    std::vector<std::unordered_map<std::string, std::optional<ValueEntry>>> transaction_stack_;

    bool isExpired(const ValueEntry& entry) const {
        if (!entry.expiry.has_value()) return false;
        return std::chrono::steady_clock::now() >= entry.expiry.value();
    }

public:
    void set(std::string key, std::string val, int ttl_seconds = -1) {
        ValueEntry entry;
        entry.value = std::move(val);
        if (ttl_seconds > 0) {
            entry.expiry = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds);
        }

        if (!transaction_stack_.empty()) {
            // Track old value in current transaction delta map for rollback
            auto& active_tx = transaction_stack_.back();
            if (!active_tx.count(key)) {
                if (db_.count(key)) active_tx[key] = db_[key];
                else active_tx[key] = std::nullopt; // Key did not exist
            }
        }
        db_[key] = std::move(entry);
    }

    std::optional<std::string> get(const std::string& key) {
        auto it = db_.find(key);
        if (it == db_.end() || isExpired(it->second)) return std::nullopt;
        return it->second.value;
    }

    void begin() {
        transaction_stack_.push_back({});
    }

    void rollback() {
        if (transaction_stack_.empty()) return;
        auto active_tx = std::move(transaction_stack_.back());
        transaction_stack_.pop_back();

        for (auto& [key, old_entry] : active_tx) {
            if (old_entry.has_value()) db_[key] = old_entry.value();
            else db_.erase(key);
        }
    }

    void commit() {
        if (transaction_stack_.empty()) return;
        transaction_stack_.clear(); // Commit flattens all transaction deltas
    }
};
```

---

#### Q4. Currency Exchange Graph (Multi-Currency Rate Calculator) [Bloomberg Financial Core]
**Question:** Given currency exchange pairs (e.g. `USD -> EUR = 0.85`, `EUR -> GBP = 0.88`), find the conversion rate between any two currencies `start` and `end`.

```cpp
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>

class CurrencyConverter {
    std::unordered_map<std::string, std::vector<std::pair<std::string, double>>> adj_;
public:
    void addRate(const std::string& from, const std::string& to, double rate) {
        adj_[from].push_back({to, rate});
        adj_[to].push_back({from, 1.0 / rate}); // Bidirectional exchange
    }

    double getRate(const std::string& from, const std::string& to) const {
        if (!adj_.count(from) || !adj_.count(to)) return -1.0;
        if (from == to) return 1.0;

        std::queue<std::pair<std::string, double>> q;
        std::unordered_set<std::string> visited;

        q.push({from, 1.0});
        visited.insert(from);

        while (!q.empty()) {
            auto [curr_curr, curr_rate] = q.front();
            q.pop();

            if (curr_curr == to) return curr_rate;

            if (adj_.count(curr_curr)) {
                for (const auto& [next_curr, edge_rate] : adj_.at(curr_curr)) {
                    if (!visited.count(next_curr)) {
                        visited.insert(next_curr);
                        q.push({next_curr, curr_rate * edge_rate});
                    }
                }
            }
        }
        return -1.0;
    }
};
```

---

#### Q5. Invalid Transactions Detector (LeetCode 1169) [Bloomberg Data Quality]
**Question:** A transaction is invalid if amount $> 1000$ or if it occurs within 60 minutes of another transaction with the same name in a different city.

```cpp
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <cmath>

struct Tx {
    std::string name, city, raw;
    int time, amount, id;
};

std::vector<std::string> invalidTransactions(const std::vector<std::string>& transactions) {
    std::vector<Tx> txs;
    std::unordered_map<std::string, std::vector<Tx>> name_map;

    for (int i = 0; i < (int)transactions.size(); ++i) {
        std::stringstream ss(transactions[i]);
        std::string name, time_str, amt_str, city;
        std::getline(ss, name, ',');
        std::getline(ss, time_str, ',');
        std::getline(ss, amt_str, ',');
        std::getline(ss, city, ',');

        Tx t{name, city, transactions[i], std::stoi(time_str), std::stoi(amt_str), i};
        txs.push_back(t);
        name_map[name].push_back(t);
    }

    std::vector<bool> is_invalid(transactions.size(), false);
    for (const auto& t : txs) {
        if (t.amount > 1000) is_invalid[t.id] = true;
        for (const auto& other : name_map[t.name]) {
            if (t.city != other.city && std::abs(t.time - other.time) <= 60) {
                is_invalid[t.id] = true;
                break;
            }
        }
    }

    std::vector<std::string> result;
    for (int i = 0; i < (int)transactions.size(); ++i) {
        if (is_invalid[i]) result.push_back(transactions[i]);
    }
    return result;
}
```

---

#### Q6. Stock Spanner / Monotonic Stack for Price Feeds (LeetCode 901) [Bloomberg Ticker Streams]
**Question:** Design a class that collects daily price quotes and returns the span of that day's price (number of consecutive days price $\le$ today's price).

```cpp
#include <stack>
#include <utility>

class StockSpanner {
    // {price, span}
    std::stack<std::pair<int, int>> st_;
public:
    int next(int price) {
        int span = 1;
        while (!st_.empty() && st_.top().first <= price) {
            span += st_.top().second;
            st_.pop();
        }
        st_.push({price, span});
        return span;
    }
};
```
- **Time Complexity:** Amortized $O(1)$ per `next()` call.

---

#### Q7. Order Book L2 Data Structure (AoS vs SoA Optimization) [Bloomberg Trading Engine]
**Question:** Compare Array of Structs (AoS) vs Struct of Arrays (SoA) for aggregating total volume in high-frequency order books.

```cpp
// Array of Structs (AoS) - Cache Bottleneck (32 bytes per Order -> 2 orders per 64B cache line)
struct OrderAoS { uint64_t id; double price; uint32_t volume; char side; uint64_t timestamp; };

// Struct of Arrays (SoA) - Cache Friendly (16 contiguous uint32_t volumes per 64B cache line)
class OrderBookSoA {
    std::vector<uint64_t> ids_;
    std::vector<double> prices_;
    std::vector<uint32_t> volumes_; // 100% L1 Cache Utilization during total volume sum!
public:
    uint64_t getTotalVolume() const {
        uint64_t total = 0;
        for (uint32_t v : volumes_) total += v;
        return total;
    }
};
```

---

#### Q8. Real-Time Market Data Stream Ticker Processor [Bloomberg Data Feeds]
**Question:** Compute trailing 5-minute Volume-Weighted Average Price (VWAP) for streaming stock price ticks.

```cpp
#include <deque>
#include <chrono>

struct TradeTick {
    double price;
    uint32_t volume;
    int timestamp_sec;
};

class VWAPCalculator {
    std::deque<TradeTick> ticks_;
    double sum_price_vol_ = 0.0;
    uint64_t total_volume_ = 0;
    static constexpr int WINDOW_SEC = 300; // 5 minutes

public:
    void addTick(double price, uint32_t volume, int timestamp_sec) {
        ticks_.push_back({price, volume, timestamp_sec});
        sum_price_vol_ += price * volume;
        total_volume_ += volume;

        // Evict ticks older than 5 minutes
        while (!ticks_.empty() && (timestamp_sec - ticks_.front().timestamp_sec > WINDOW_SEC)) {
            sum_price_vol_ -= ticks_.front().price * ticks_.front().volume;
            total_volume_ -= ticks_.front().volume;
            ticks_.pop_front();
        }
    }

    double getVWAP() const {
        if (total_volume_ == 0) return 0.0;
        return sum_price_vol_ / total_volume_;
    }
};
```

---

#### Q9. Lock-Free SPSC Ring Buffer for High-Throughput Market Feeds [Bloomberg Low-Latency]
**Question:** Write a Single-Producer Single-Consumer (SPSC) lock-free queue with cache-line alignment to eliminate false sharing.

```cpp
#include <atomic>
#include <array>
#include <optional>
#include <new>

template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    alignas(64) std::atomic<size_t> head_{0}; // Consumer index (Cache Line 1)
    alignas(64) std::atomic<size_t> tail_{0}; // Producer index (Cache Line 2)
    alignas(64) std::array<T, Capacity> buffer_;

public:
    bool push(const T& item) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (t - h >= Capacity) return false; // Full
        buffer_[t & (Capacity - 1)] = item;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        if (h == t) return std::nullopt; // Empty
        T item = buffer_[h & (Capacity - 1)];
        head_.store(h + 1, std::memory_order_release);
        return item;
    }
};
```

---

#### Q10. Custom Arena Allocator for Zero-Heap Allocations [Bloomberg Memory Pools]
**Question:** Write a stack-backed linear arena allocator providing $O(1)$ zero-syscall allocations.

```cpp
#include <cstddef>
#include <new>
#include <utility>

template <size_t Size>
class FixedArena {
    alignas(std::max_align_t) char buffer_[Size];
    size_t offset_ = 0;
public:
    void* allocate(size_t n, size_t alignment = alignof(std::max_align_t)) {
        size_t current_addr = reinterpret_cast<size_t>(buffer_ + offset_);
        size_t aligned_addr = (current_addr + alignment - 1) & ~(alignment - 1);
        size_t next_offset = (aligned_addr - reinterpret_cast<size_t>(buffer_)) + n;
        if (next_offset > Size) throw std::bad_alloc();
        offset_ = next_offset;
        return reinterpret_cast<void*>(aligned_addr);
    }
    void reset() noexcept { offset_ = 0; }
};
```

---

### Category 2: C++ Language Mechanics & Memory Management

#### Q11. `std::shared_ptr` Control Block & `enable_shared_from_this` Mechanics
- **Q:** How does `enable_shared_from_this` prevent double deletion?
- **A:** It adds an internal private `std::weak_ptr<T>` to the object. When wrapped in a `shared_ptr`, the constructor initializes this `weak_ptr`. Calling `shared_from_this()` promotes the `weak_ptr` to a `shared_ptr`, reusing the existing control block instead of allocating a duplicate one.

#### Q12. vtable Layout with Multiple Inheritance & Adjustor Thunks
- **Q:** How does a virtual function call modify the `this` pointer in multiple inheritance?
- **A:** In `class Derived : public BaseA, public BaseB`, `Derived` contains two `vptr`s. When calling an overridden method through a `BaseB*` pointer, the vtable entry points to an **adjustor thunk** instruction sequence that subtracts `sizeof(BaseA)` from `this` before invoking `Derived::method()`.

#### Q13. Custom Deleter for C-Style File & Database Handles
```cpp
#include <memory>
#include <cstdio>

// Custom Deleter using Lambda
auto make_scoped_file(const char* path, const char* mode) {
    return std::unique_ptr<FILE, decltype(&std::fclose)>(
        std::fopen(path, mode), &std::fclose
    );
}
```

#### Q14. Exception Safety Guarantees & Copy-and-Swap Idiom
```cpp
class SafeBuffer {
    size_t size_;
    int* data_;
public:
    SafeBuffer(size_t s) : size_(s), data_(new int[s]) {}
    ~SafeBuffer() { delete[] data_; }
    
    // Copy Constructor
    SafeBuffer(const SafeBuffer& other) : size_(other.size_), data_(new int[other.size_]) {
        std::copy(other.data_, other.data_ + size_, data_);
    }

    friend void swap(SafeBuffer& a, SafeBuffer& b) noexcept {
        using std::swap;
        swap(a.size_, b.size_);
        swap(a.data_, b.data_);
    }

    // Strong Exception Safety Assignment via Copy-and-Swap
    SafeBuffer& operator=(SafeBuffer other) noexcept {
        swap(*this, other);
        return *this;
    }
};
```

#### Q15. Pimpl Idiom Implementation for Binary ABI Firewalls
```cpp
// Header: MarketFeed.h
#include <memory>
class MarketFeed {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    MarketFeed();
    ~MarketFeed(); // MUST be declared in header, defined in .cpp!
    void connect();
};
```

#### Q16. Compile-Time Financial Lookup Tables (`constexpr` vs `consteval`)
```cpp
// Guaranteed compile-time execution ONLY (C++20 consteval)
consteval double calc_fixed_margin(double price) {
    return price * 0.05 + 0.10;
}
constexpr double margin = calc_fixed_margin(100.0); // 5.10 computed at compile time!
```

#### Q17. Small String Optimization (SSO) & POSIX `string_view` Traps
- **Trap:** Passing `.data()` from a non-null-terminated `std::string_view` into C-style APIs (`fopen`, `printf`) reads memory past the view boundary.
- **Fix:** Construct a `std::string(view)` before passing to C-style APIs expecting `\0`.

#### Q18. RTTI Mechanics in `dynamic_cast`
- **Q:** What is required for `dynamic_cast` to compile and execute?
- **A:** The base class must have at least one virtual function to enable RTTI (`typeinfo`). `dynamic_cast` traverses the `typeinfo` hierarchy at runtime. Fails return `nullptr` for pointers and throw `std::bad_cast` for references.

#### Q19. Rule of Zero vs Rule of Five
- **Rule of Zero:** Use standard RAII types (`std::string`, `vector`, `unique_ptr`). Declare NO custom destructors or copy/move constructors.
- **Rule of Five:** If a class manages a raw resource, explicitly declare/delete all 5: Destructor, Copy Constructor, Copy Assignment, Move Constructor, Move Assignment.

#### Q20. Why Destructors Default to `noexcept(true)`
- **Q:** What happens if a destructor throws during exception unwinding?
- **A:** Two active exceptions exist simultaneously in the thread $\rightarrow$ `std::terminate()` is invoked immediately, crashing the process.

---

### Category 3: Concurrency, Multithreading & Synchronization

#### Q21. Thread-Safe Bounded Queue (Producer-Consumer Pattern)
```cpp
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class BoundedQueue {
    std::queue<T> q_;
    size_t capacity_;
    std::mutex mtx_;
    std::condition_variable cv_push_, cv_pop_;
public:
    explicit BoundedQueue(size_t cap) : capacity_(cap) {}

    void push(T val) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_push_.wait(lock, [&]{ return q_.size() < capacity_; });
        q_.push(std::move(val));
        cv_pop_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_pop_.wait(lock, [&]{ return !q_.empty(); });
        T val = std::move(q_.front());
        q_.pop();
        cv_push_.notify_one();
        return val;
    }
};
```

#### Q22. Reader-Writer Lock using `std::shared_mutex` (C++17)
```cpp
#include <shared_mutex>
#include <unordered_map>

class SharedMarketData {
    std::unordered_map<std::string, double> prices_;
    mutable std::shared_mutex rw_mtx_;
public:
    double getPrice(const std::string& symbol) const {
        std::shared_lock<std::shared_mutex> lock(rw_mtx_); // Shared Read Lock
        return prices_.at(symbol);
    }
    void updatePrice(const std::string& symbol, double price) {
        std::unique_lock<std::shared_mutex> lock(rw_mtx_); // Exclusive Write Lock
        prices_[symbol] = price;
    }
};
```

#### Q23. Futex Mechanics & `std::mutex` vs `std::atomic`
- **Futex:** Performs atomic CAS in userspace (~5ns). Takes kernel lock path (`sys_futex`) ONLY on contention.
- **Atomics:** Hardware bus-level operations (`lock cmpxchg`) for primitive types without thread context switching.

#### Q24. Spurious Wakeups & Condition Variable Predicate Loop
- **Q:** Why must `cv.wait()` be wrapped inside a predicate loop?
- **A:** OS signals CV threads without notifications (Spurious Wakeups). The loop `[&]{ return predicate; }` re-checks the state before proceeding.

#### Q25. Thread-Safe Event Bus with `std::weak_ptr`
```cpp
#include <vector>
#include <memory>
#include <mutex>

class EventObserver { public: virtual ~EventObserver() = default; virtual void onEvent() = 0; };

class EventBus {
    std::vector<std::weak_ptr<EventObserver>> observers_;
    std::mutex mtx_;
public:
    void publish() {
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto it = observers_.begin(); it != observers_.end(); ) {
            if (auto sp = it->lock()) {
                sp->onEvent();
                ++it;
            } else {
                it = observers_.erase(it); // Auto-prune dead observers
            }
        }
    }
};
```

#### Q26. `std::atomic` Memory Orderings (`relaxed`, `acquire/release`, `seq_cst`)
- `relaxed`: Atomic operation without synchronization fences.
- `acquire/release`: Establishes producer-consumer happens-before relationships between threads.
- `seq_cst`: Total global ordering of operations across all cores.

#### Q27. False Sharing Mitigation via `alignas(64)`
```cpp
struct alignas(64) ThreadCounter {
    std::atomic<uint64_t> count{0};
}; // Prevents 2 core L1 caches from bouncing the same 64B cache line
```

#### Q28. Deadlock Avoidance with `std::scoped_lock` (C++17)
```cpp
std::mutex m1, m2;
void safe_transfer() {
    std::scoped_lock lock(m1, m2); // Locks both mutexes atomically without deadlock!
}
```

#### Q29. Thread Pool Implementation Sketch
```cpp
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;
public:
    explicit ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
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
    ~ThreadPool() {
        { std::unique_lock<std::mutex> lock(mtx_); stop_ = true; }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }
};
```

#### Q30. `std::jthread` & Cooperative Cancellation (`std::stop_token`) (C++20)
```cpp
#include <thread>
#include <iostream>

void worker(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        // Do work until cancellation requested
    }
}
int main() {
    std::jthread t(worker); // Auto-joins on destructor scope exit!
}
```

---

### Category 4: STL Containers, Iterators & Algorithm Mechanics

#### Q31. `std::map` (Red-Black Tree) vs `std::unordered_map` (Hash Table)
- `std::map`: Sorted keys, $O(\log N)$ operations, no rehash overhead.
- `std::unordered_map`: Unsorted keys, $O(1)$ average lookup, rehashes when load factor $\alpha > 1.0$, $O(N)$ worst-case under hash collisions.

#### Q32. Comprehensive Iterator Invalidation Matrix
- `vector`: Realloc invalidates ALL iterators/refs; insert/erase invalidates from target point to `end()`.
- `deque`: `push_front`/`push_back` invalidates iterators (references remain valid!).
- `list` / `map`: `insert` NEVER invalidates iterators/refs; `erase` invalidates only erased elements.

#### Q33. `emplace_back` vs `push_back` Placement New Mechanics
- `emplace_back` forwards arguments directly into placement `new` inside vector memory, avoiding temporary object construction.

#### Q34. Remove-Erase Idiom Mechanics
```cpp
// Erases all odd elements efficiently
v.erase(std::remove_if(v.begin(), v.end(), [](int x) { return x % 2 != 0; }), v.end());
```

#### Q35. Custom Hash Function for User-Defined Structs
```cpp
struct OrderKey { int id; std::string symbol; };
struct OrderKeyHash {
    size_t operator()(const OrderKey& k) const {
        return std::hash<int>{}(k.id) ^ (std::hash<std::string>{}(k.symbol) << 1);
    }
};
std::unordered_map<OrderKey, double, OrderKeyHash> key_map;
```

#### Q36. `std::priority_queue` Complexity & Custom Comparators
- Binary Max-Heap: `push` $O(\log N)$, `top` $O(1)$, `pop` $O(\log N)$.
```cpp
auto cmp = [](int a, int b) { return a > b; }; // Min-heap comparator
std::priority_queue<int, std::vector<int>, decltype(cmp)> min_heap(cmp);
```

#### Q37. Strict Weak Ordering Comparator Requirements
- `comp(x, x)` MUST return `false` (Irreflexivity). Using `<=` in `std::sort` violates irreflexivity $\rightarrow$ Heap Corruption UB!

#### Q38. `std::lower_bound` vs `std::upper_bound` Binary Search
- `lower_bound`: First position $\ge$ target.
- `upper_bound`: First position $>$ target.

#### Q39. `std::span` Non-Owning Contiguous Buffer Views (C++20)
```cpp
#include <span>
void process_bytes(std::span<const uint8_t> buffer) {
    // Accepts std::vector, std::array, or C-array with 0 allocations!
}
```

#### Q40. All $O(1)$ Data Structure (LeetCode 432)
- **Design:** Doubly-linked list of key buckets (sorted by count) + `unordered_map` mapping key $\rightarrow$ list node iterator for $O(1)$ `inc`, `dec`, `getMaxKey`, `getMinKey`.

---

### Category 5: Advanced Systems, Memory Layouts & Gotchas

#### Q41. Small Buffer Optimization (SBO) in Custom Type Erasure (`std::function`)
- Stores small closure objects directly inside an inline 16–32 byte stack buffer, bypassing dynamic heap allocations.

#### Q42. `std::expected` (C++23) vs `std::error_code` Monadic Error Handling
- Non-allocating tagged union monadic error propagation without stack unwinding overhead.

#### Q43. Memory Leak & Corruption Debugging with ASan & GDB Core Dumps
- Compile with `-fsanitize=address -g`. Inspect core dumps in GDB using `bt` and `thread apply all bt`.

#### Q44. Modern CMake Target Visibility (`PUBLIC`, `PRIVATE`, `INTERFACE`)
- `PRIVATE`: Target internal dependencies.
- `PUBLIC`: Target and downstream consumer dependencies.
- `INTERFACE`: Downstream header-only dependencies.

#### Q45. Struct Alignment & Padding Rules (`alignof`, `alignas`, EBO)
- Order struct fields from largest alignment to smallest to reduce padding overhead up to 50%.

#### Q46. Placement `new` & Explicit Destructor Call Mechanics
```cpp
alignas(alignof(Widget)) char buf[sizeof(Widget)];
Widget* w = new (buf) Widget(); // Placement new
w->~Widget();                    // Explicit destructor
```

#### Q47. `std::move` Pessimization on Return Values Preventing NRVO
- **Anti-Pattern:** `return std::move(local_var);` forces a move constructor call and prevents $0$-cost NRVO copy elision!

#### Q48. Top-Level Const vs Low-Level Const
- `const int* p`: Low-Level const (pointed-to object is const).
- `int* const p`: Top-Level const (pointer variable itself is const).

#### Q49. Modern C++20 Ranges Pipeline Lazy Evaluation
```cpp
auto view = nums | std::views::filter([](int x) { return x % 2 == 0; }); // Zero allocations!
```

#### Q50. C++23 Explicit Object Parameters (`deducing this`)
```cpp
struct Data {
    template <typename Self>
    auto&& get(this Self&& self) { return std::forward<Self>(self).value_; }
    int value_;
};
```

#### Q51. `volatile` vs `std::atomic` in Multithreading
- `volatile`: Memory-Mapped I/O hardware register reads.
- `std::atomic`: Multithreaded lock-free data synchronization and memory fences.

#### Q52. Intrusive Doubly-Linked List Memory Pool Design
- Embed `next`/`prev` pointers directly inside pooled objects for cache-friendly $O(1)$ memory pool allocations.

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
