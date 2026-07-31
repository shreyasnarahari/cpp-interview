# Bloomberg — Design Questions

> **Focus:** System design and data structure design problems with C++ implementations, tailored to Bloomberg's financial systems domain.
>
> For pure C++ language mechanics questions, see [bloomberg-cpp.md](./bloomberg-cpp.md).

---

## Overview

Bloomberg's design interviews test your ability to architect components that handle real-time financial data. Questions combine data structure choice, algorithmic thinking, and systems awareness (latency, throughput, memory). Expect to implement working C++ code, not just whiteboard boxes.

| # | Problem | Domain | Key Data Structures |
|---|---|---|---|
| D1 | Underground System | Real-Time Tracking | `unordered_map`, structured bindings |
| D2 | Leaderboard | Ranking / Top-K | `multiset`, `unordered_map` |
| D3 | Key-Value Store with TTL & Transactions | Core Infrastructure | `unordered_map`, transaction stack |
| D4 | Currency Exchange Graph | Financial Core | BFS on adjacency list |
| D5 | Invalid Transaction Detector | Data Quality | Grouping + time-window scan |
| D6 | Stock Price Spanner | Streaming Data | Monotonic stack |
| D7 | Order Book (AoS vs SoA) | Trading Engine | Cache-aware data layout |
| D8 | VWAP Calculator | Market Data | Sliding window with `deque` |
| D9 | All O(1) Data Structure | General | Doubly-linked list + hash map |
| D10 | LRU Cache | Caching | `list` + `unordered_map` |
| D11 | Intrusive Linked List Pool | Memory Management | Embedded pointers |
| D12 | Rate Limiter | API Systems | Token bucket / sliding window |

---

## D1. Underground System (`checkIn`, `checkOut`, `getAverageTime`)

**Question:** Design a system that tracks passenger travel times between stations and calculates the average travel duration between any two stations. All operations must run in O(1) average time.

**Why Bloomberg asks this:** Tests your ability to maintain running statistics on streaming event pairs — similar to tracking trade execution latency between venues.

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

        check_ins_.erase(it);
    }

    double getAverageTime(const std::string& startStation, 
                          const std::string& endStation) const {
        std::string route = startStation + "->" + endStation;
        auto it = route_stats_.find(route);
        if (it == route_stats_.end()) return 0.0;
        return it->second.first / it->second.second;
    }
};
```

**Complexity:** O(1) average for all operations. Space O(P + R) where P = active passengers, R = distinct routes.

**Design discussion points:**
- **String concatenation for route key** is O(L) where L = station name length. For ultra-low-latency, you could hash both station names independently and combine: `hash(from) ^ (hash(to) << 32)`.
- **Thread safety:** In production, wrap with `shared_mutex` — reads (`getAverageTime`) use `shared_lock`, writes use `unique_lock`.
- **Extension:** Support removing stale passengers (timeout after X minutes without checkout).

---

## D2. Leaderboard (`addScore`, `top`, `reset`)

**Question:** Design a Leaderboard supporting `addScore(playerId, score)` that accumulates scores, `top(K)` returning the sum of top K scores, and `reset(playerId)` to remove a player.

**Why Bloomberg asks this:** Directly maps to ranked instrument lists (top performers by return, top sectors by volume).

```cpp
#include <unordered_map>
#include <set>

class Leaderboard {
    std::unordered_map<int, int> player_scores_;
    std::multiset<int, std::greater<int>> sorted_scores_;

public:
    void addScore(int playerId, int score) {
        if (auto it = player_scores_.find(playerId); it != player_scores_.end()) {
            // Remove old score from sorted set
            sorted_scores_.erase(sorted_scores_.find(it->second));
            it->second += score;
            sorted_scores_.insert(it->second);
        } else {
            player_scores_[playerId] = score;
            sorted_scores_.insert(score);
        }
    }

    int top(int K) {
        int sum = 0, count = 0;
        for (int score : sorted_scores_) {
            sum += score;
            if (++count == K) break;
        }
        return sum;
    }

    void reset(int playerId) {
        if (auto it = player_scores_.find(playerId); it != player_scores_.end()) {
            sorted_scores_.erase(sorted_scores_.find(it->second));
            player_scores_.erase(it);
        }
    }
};
```

**Complexity:** `addScore` O(log N), `reset` O(log N), `top(K)` O(K).

**Design discussion points:**
- **Alternative with `std::priority_queue`:** Can't efficiently remove arbitrary elements. `multiset` supports O(log N) removal by iterator.
- **For top(K) in O(1):** Maintain a running sum of all scores + an augmented order-statistic tree.
- **Bloomberg extension:** "Top K instruments by volume in a rolling 5-minute window" — combine with sliding window eviction.

---

## D3. In-Memory Key-Value Store with TTL & Transactions

**Question:** Implement an in-memory key-value store supporting `set(key, val, ttl)`, `get(key)` with TTL-based expiry, and nested transactions (`begin()`, `commit()`, `rollback()`).

**Why Bloomberg asks this:** Tests design of configuration stores, session management, and transactional data layers — all common in financial middleware.

```cpp
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

class TransactionalKV {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct Entry {
        std::string value;
        std::optional<TimePoint> expiry;
        
        bool isExpired() const {
            return expiry.has_value() && Clock::now() >= *expiry;
        }
    };

    std::unordered_map<std::string, Entry> db_;
    
    // Transaction stack: each frame maps key -> previous value (for rollback)
    // nullopt means the key didn't exist before this transaction
    std::vector<std::unordered_map<std::string, std::optional<Entry>>> tx_stack_;

    void saveForRollback(const std::string& key) {
        if (tx_stack_.empty()) return;
        auto& frame = tx_stack_.back();
        if (frame.count(key)) return;  // already saved in this transaction
        
        auto it = db_.find(key);
        frame[key] = (it != db_.end()) ? std::optional{it->second} : std::nullopt;
    }

public:
    void set(const std::string& key, std::string val, int ttl_seconds = -1) {
        saveForRollback(key);
        
        Entry entry;
        entry.value = std::move(val);
        if (ttl_seconds > 0) {
            entry.expiry = Clock::now() + std::chrono::seconds(ttl_seconds);
        }
        db_[key] = std::move(entry);
    }

    std::optional<std::string> get(const std::string& key) const {
        auto it = db_.find(key);
        if (it == db_.end() || it->second.isExpired()) return std::nullopt;
        return it->second.value;
    }

    bool del(const std::string& key) {
        saveForRollback(key);
        return db_.erase(key) > 0;
    }

    void begin() {
        tx_stack_.emplace_back();
    }

    bool rollback() {
        if (tx_stack_.empty()) return false;
        auto frame = std::move(tx_stack_.back());
        tx_stack_.pop_back();

        for (auto& [key, old_entry] : frame) {
            if (old_entry.has_value()) db_[key] = std::move(*old_entry);
            else db_.erase(key);
        }
        return true;
    }

    bool commit() {
        if (tx_stack_.empty()) return false;
        if (tx_stack_.size() == 1) {
            tx_stack_.pop_back();  // top-level commit — changes are permanent
        } else {
            // Nested commit — merge into parent transaction
            auto frame = std::move(tx_stack_.back());
            tx_stack_.pop_back();
            for (auto& [key, entry] : frame) {
                if (!tx_stack_.back().count(key)) {
                    tx_stack_.back()[key] = std::move(entry);
                }
            }
        }
        return true;
    }
};
```

**Design discussion points:**
- **Lazy vs eager expiry:** This implementation checks expiry on `get()` (lazy). Eager expiry uses a priority queue sorted by expiry time, with a background thread evicting stale entries.
- **Nested transactions:** `commit()` merges the current frame into the parent's undo log. `rollback()` at depth 2 restores state to depth 1, not to the initial state.
- **Bloomberg extension:** Add `subscribe(key, callback)` for change notifications — useful for config propagation.

---

## D4. Currency Exchange Graph

**Question:** Given currency exchange pairs (e.g., `USD -> EUR = 0.85`, `EUR -> GBP = 0.88`), find the conversion rate between any two currencies. Return -1.0 if no path exists.

**Why Bloomberg asks this:** Direct application to FX trading desks. Bloomberg Terminal users query cross-currency rates constantly.

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
        adj_[to].push_back({from, 1.0 / rate});
    }

    double getRate(const std::string& from, const std::string& to) const {
        if (!adj_.count(from) || !adj_.count(to)) return -1.0;
        if (from == to) return 1.0;

        std::queue<std::pair<std::string, double>> q;
        std::unordered_set<std::string> visited;

        q.push({from, 1.0});
        visited.insert(from);

        while (!q.empty()) {
            auto [curr, rate] = q.front();
            q.pop();

            if (curr == to) return rate;

            for (const auto& [next, edge_rate] : adj_.at(curr)) {
                if (!visited.count(next)) {
                    visited.insert(next);
                    q.push({next, rate * edge_rate});
                }
            }
        }
        return -1.0;
    }
};
```

**Complexity:** `addRate` O(1), `getRate` O(V + E) BFS.

**Design discussion points:**
- **Floating-point accumulation error:** For long chains (USD→EUR→GBP→JPY→...), multiplication errors compound. Use `long double` or log-space: `log(rate1) + log(rate2)` → `exp(sum)`.
- **Arbitrage detection:** If a cycle in the graph has a product > 1.0, an arbitrage opportunity exists. Detect with Bellman-Ford on log-rates (negative cycle = arbitrage).
- **Real-time updates:** If rates change, rebuild affected edges. Consider Union-Find for connected component queries.

---

## D5. Invalid Transaction Detector

**Question:** A transaction `[name, time, amount, city]` is invalid if `amount > 1000` OR if it occurs within 60 minutes of another transaction with the same name in a different city.

```cpp
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <cmath>

struct Transaction {
    std::string name, city, raw;
    int time, amount, index;
};

std::vector<std::string> invalidTransactions(const std::vector<std::string>& transactions) {
    std::vector<Transaction> all;
    std::unordered_map<std::string, std::vector<Transaction>> by_name;

    for (int i = 0; i < (int)transactions.size(); ++i) {
        std::stringstream ss(transactions[i]);
        std::string name, time_s, amt_s, city;
        std::getline(ss, name, ',');
        std::getline(ss, time_s, ',');
        std::getline(ss, amt_s, ',');
        std::getline(ss, city, ',');

        Transaction t{name, city, transactions[i], std::stoi(time_s), std::stoi(amt_s), i};
        all.push_back(t);
        by_name[name].push_back(t);
    }

    std::vector<bool> invalid(transactions.size(), false);
    for (const auto& t : all) {
        if (t.amount > 1000) invalid[t.index] = true;
        for (const auto& other : by_name[t.name]) {
            if (t.city != other.city && std::abs(t.time - other.time) <= 60) {
                invalid[t.index] = true;
                invalid[other.index] = true;  // mark both as invalid
                break;
            }
        }
    }

    std::vector<std::string> result;
    for (int i = 0; i < (int)transactions.size(); ++i) {
        if (invalid[i]) result.push_back(transactions[i]);
    }
    return result;
}
```

**Complexity:** O(N × M) where M = max transactions per name. For large inputs, sort by time per name and use two-pointer for O(N log N).

**Design discussion points:**
- **Optimization for sorted data:** Sort transactions by time per name. Use sliding window to find all transactions within 60-minute range — reduces from O(M²) to O(M log M) per name.
- **Bloomberg extension:** "Flag trades that exceed the 99th percentile volume for that ticker in the last hour" — rolling percentile with order-statistic tree.

---

## D6. Stock Price Spanner (Monotonic Stack)

**Question:** Design a class that collects daily price quotes and returns the "span" — the number of consecutive previous days where the price was ≤ today's price.

**Why Bloomberg asks this:** Monotonic stacks are fundamental to streaming financial analytics (support/resistance levels, consecutive trading day patterns).

```cpp
#include <stack>
#include <utility>

class StockSpanner {
    // {price, span} — span includes all previously absorbed entries
    std::stack<std::pair<int, int>> st_;
public:
    int next(int price) {
        int span = 1;
        // Absorb all previous entries with price <= current
        while (!st_.empty() && st_.top().first <= price) {
            span += st_.top().second;
            st_.pop();
        }
        st_.push({price, span});
        return span;
    }
};

// Example:
// next(100) → 1 (no previous days)
// next(80)  → 1 (80 > 100? no → span 1)
// next(60)  → 1
// next(70)  → 2 (70 >= 60 → absorb, span = 1 + 1 = 2)
// next(60)  → 1
// next(75)  → 4 (75 >= 60 → absorb(1), 75 >= 70 → absorb(2), span = 1+1+2 = 4)
```

**Complexity:** Amortized O(1) per `next()` — each element is pushed and popped at most once.

**Design discussion points:**
- **Extension to streaming:** Works naturally with unbounded streams — no need to store all historical data.
- **Bidirectional:** "How many consecutive days in BOTH directions is price ≤ today?" — run the monotonic stack forward and backward.

---

## D7. Order Book — AoS vs SoA Optimization

**Question:** Compare Array of Structs (AoS) vs Struct of Arrays (SoA) for aggregating total volume in a high-frequency order book. Why does SoA win for column-wise operations?

```cpp
// ---- Array of Structs (AoS) ----
// 32 bytes per Order → 2 orders per 64-byte cache line
struct OrderAoS { 
    uint64_t id;          // 8 bytes
    double price;         // 8 bytes
    uint32_t volume;      // 4 bytes
    char side;            // 1 byte + 3 padding
    uint64_t timestamp;   // 8 bytes
};
// Total: 32 bytes

// When summing volumes, we load 32 bytes per order but only use 4 → 12.5% cache utilization!

// ---- Struct of Arrays (SoA) ----
// Volumes are contiguous → 16 uint32_t per 64-byte cache line
class OrderBookSoA {
    std::vector<uint64_t> ids_;
    std::vector<double> prices_;
    std::vector<uint32_t> volumes_;   // 100% L1 cache utilization for volume sum!
    std::vector<char> sides_;
    std::vector<uint64_t> timestamps_;

public:
    void addOrder(uint64_t id, double price, uint32_t vol, char side, uint64_t ts) {
        ids_.push_back(id);
        prices_.push_back(price);
        volumes_.push_back(vol);
        sides_.push_back(side);
        timestamps_.push_back(ts);
    }

    uint64_t getTotalVolume() const {
        uint64_t total = 0;
        for (uint32_t v : volumes_) total += v;  // sequential, prefetcher-friendly
        return total;
        // Compiler can auto-vectorize this with SIMD (AVX2: 8 volumes per cycle)
    }

    double getVWAP() const {
        double sum_pv = 0.0;
        uint64_t sum_v = 0;
        for (size_t i = 0; i < prices_.size(); ++i) {
            sum_pv += prices_[i] * volumes_[i];
            sum_v += volumes_[i];
        }
        return sum_v > 0 ? sum_pv / sum_v : 0.0;
    }
};
```

**When to use which:**
| Pattern | Best Layout | Why |
|---|---|---|
| Sum all volumes | SoA | Sequential scan of one array |
| Process one order (all fields) | AoS | All fields in one cache line |
| Filter by price range | SoA | Scan prices array only |
| Send full order over network | AoS | Contiguous serialization |

---

## D8. Real-Time VWAP Calculator (Sliding Window)

**Question:** Compute trailing 5-minute Volume-Weighted Average Price (VWAP) for streaming stock price ticks. All operations must be O(1) amortized.

**Why Bloomberg asks this:** VWAP is one of the most common trading benchmarks. Bloomberg Terminal computes VWAP in real-time for every listed security.

```cpp
#include <deque>

struct TradeTick {
    double price;
    uint32_t volume;
    int timestamp_sec;
};

class VWAPCalculator {
    std::deque<TradeTick> window_;
    double sum_price_vol_ = 0.0;     // Σ(price × volume)
    uint64_t sum_volume_ = 0;        // Σ(volume)
    static constexpr int WINDOW_SEC = 300;  // 5 minutes

public:
    void addTick(double price, uint32_t volume, int timestamp_sec) {
        // Add new tick
        window_.push_back({price, volume, timestamp_sec});
        sum_price_vol_ += price * volume;
        sum_volume_ += volume;

        // Evict expired ticks
        while (!window_.empty() && 
               (timestamp_sec - window_.front().timestamp_sec > WINDOW_SEC)) {
            sum_price_vol_ -= window_.front().price * window_.front().volume;
            sum_volume_ -= window_.front().volume;
            window_.pop_front();
        }
    }

    double getVWAP() const {
        if (sum_volume_ == 0) return 0.0;
        return sum_price_vol_ / sum_volume_;
    }

    size_t windowSize() const { return window_.size(); }
};
```

**Complexity:** Amortized O(1) for `addTick` (each tick is added and removed exactly once). O(1) for `getVWAP`.

**Design discussion points:**
- **Floating-point drift:** Subtracting small numbers from large running sums causes precision loss. Periodically recompute from scratch: `if (window_.size() % 10000 == 0) recalculate();`
- **Thread safety:** Producer thread calls `addTick`, consumer calls `getVWAP`. Use `shared_mutex` or make it lock-free with atomic snapshot reads.
- **Extension:** Support multiple securities — `unordered_map<string, VWAPCalculator>`.

---

## D9. All O(1) Data Structure

**Question:** Design a data structure supporting `inc(key)`, `dec(key)`, `getMaxKey()`, `getMinKey()` — all in O(1).

**Why Bloomberg asks this:** Tests advanced data structure composition. Maps to real-time "most active ticker" tracking.

```
Architecture: Doubly-linked list of COUNT BUCKETS + hash map for O(1) key lookup

Hash Map:           Doubly-Linked List (sorted by count):
key → node_iter     HEAD ↔ [count=1: {a,b}] ↔ [count=3: {c}] ↔ [count=5: {d,e}] ↔ TAIL
                            min_key                                    max_key
```

```cpp
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <string>

class AllOne {
    struct Bucket {
        int count;
        std::unordered_set<std::string> keys;
    };

    std::list<Bucket> buckets_;  // sorted by count
    std::unordered_map<std::string, std::list<Bucket>::iterator> key_to_bucket_;

public:
    void inc(const std::string& key) {
        if (!key_to_bucket_.count(key)) {
            // New key — insert at front with count 1
            if (buckets_.empty() || buckets_.front().count != 1) {
                buckets_.push_front({1, {}});
            }
            buckets_.front().keys.insert(key);
            key_to_bucket_[key] = buckets_.begin();
        } else {
            auto curr = key_to_bucket_[key];
            int new_count = curr->count + 1;
            auto next = std::next(curr);
            
            if (next == buckets_.end() || next->count != new_count) {
                next = buckets_.insert(next, {new_count, {}});
            }
            next->keys.insert(key);
            key_to_bucket_[key] = next;
            
            curr->keys.erase(key);
            if (curr->keys.empty()) buckets_.erase(curr);
        }
    }

    void dec(const std::string& key) {
        if (!key_to_bucket_.count(key)) return;
        
        auto curr = key_to_bucket_[key];
        int new_count = curr->count - 1;
        
        if (new_count == 0) {
            key_to_bucket_.erase(key);
        } else {
            auto prev = (curr != buckets_.begin()) ? std::prev(curr) : buckets_.end();
            if (prev == buckets_.end() || prev->count != new_count) {
                prev = buckets_.insert(curr, {new_count, {}});
            }
            prev->keys.insert(key);
            key_to_bucket_[key] = prev;
        }
        
        curr->keys.erase(key);
        if (curr->keys.empty()) buckets_.erase(curr);
    }

    std::string getMaxKey() const {
        return buckets_.empty() ? "" : *buckets_.back().keys.begin();
    }

    std::string getMinKey() const {
        return buckets_.empty() ? "" : *buckets_.front().keys.begin();
    }
};
```

**Complexity:** All operations O(1).

---

## D10. LRU Cache

**Question:** Design a Least Recently Used cache with O(1) `get` and `put`.

**Why Bloomberg asks this:** Caching is everywhere — market data snapshots, user session data, configuration.

```cpp
#include <unordered_map>
#include <list>

class LRUCache {
    int capacity_;
    std::list<std::pair<int, int>> order_;  // {key, value}, front = most recent
    std::unordered_map<int, std::list<std::pair<int, int>>::iterator> map_;

public:
    explicit LRUCache(int cap) : capacity_(cap) {}

    int get(int key) {
        auto it = map_.find(key);
        if (it == map_.end()) return -1;
        
        // Move to front (most recently used)
        order_.splice(order_.begin(), order_, it->second);
        return it->second->second;
    }

    void put(int key, int value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            order_.splice(order_.begin(), order_, it->second);
            return;
        }
        
        if ((int)map_.size() >= capacity_) {
            // Evict least recently used (back of list)
            int evict_key = order_.back().first;
            order_.pop_back();
            map_.erase(evict_key);
        }
        
        order_.push_front({key, value});
        map_[key] = order_.begin();
    }
};
```

**Design discussion points:**
- **`std::list::splice` is O(1)** — moves a node within the list without copying.
- **Thread-safe variant:** Wrap with `shared_mutex`; `get` takes shared lock, `put` takes exclusive lock.
- **Bloomberg extension:** Add TTL expiry — each entry has a timestamp, background thread evicts stale entries.

---

## D11. Intrusive Linked List Memory Pool

**Question:** Design a fixed-size object pool using an intrusive free list — O(1) allocate and deallocate with zero `malloc` calls after initialization.

**Why Bloomberg asks this:** Low-latency trading systems pre-allocate all memory at startup.

```cpp
#include <cstddef>
#include <new>
#include <cassert>

template <typename T, size_t PoolSize>
class ObjectPool {
    union Slot {
        T object;
        Slot* next;           // intrusive — next pointer embedded in the slot itself
        Slot() : next(nullptr) {}
        ~Slot() {}
    };

    alignas(alignof(T)) Slot pool_[PoolSize];
    Slot* free_head_ = nullptr;

public:
    ObjectPool() {
        // Build free list linking all slots
        for (size_t i = 0; i < PoolSize; ++i) {
            pool_[i].next = free_head_;
            free_head_ = &pool_[i];
        }
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        assert(free_head_ != nullptr && "Pool exhausted");
        Slot* slot = free_head_;
        free_head_ = slot->next;
        return new (&slot->object) T(std::forward<Args>(args)...);  // placement new
    }

    void deallocate(T* ptr) {
        ptr->~T();  // explicit destructor call
        Slot* slot = reinterpret_cast<Slot*>(ptr);
        slot->next = free_head_;
        free_head_ = slot;
    }
};

// Usage:
// ObjectPool<Order, 100000> order_pool;
// Order* o = order_pool.allocate(id, price, volume);
// order_pool.deallocate(o);
```

**Key properties:**
- **Zero heap allocations** after construction
- **O(1) allocate and deallocate** — just pointer arithmetic
- **Cache-friendly** — objects are contiguous in memory

---

## D12. Token Bucket Rate Limiter

**Question:** Implement a rate limiter that allows at most N requests per second, with burst capacity.

**Why Bloomberg asks this:** API rate limiting for Bloomberg Terminal data feeds, preventing abuse of real-time data endpoints.

```cpp
#include <chrono>
#include <algorithm>

class RateLimiter {
    double tokens_;
    double max_tokens_;
    double refill_rate_;  // tokens per second
    std::chrono::steady_clock::time_point last_refill_;

public:
    // max_requests: burst capacity, per_second: sustained rate
    RateLimiter(double max_requests, double per_second)
        : tokens_(max_requests)
        , max_tokens_(max_requests)
        , refill_rate_(per_second)
        , last_refill_(std::chrono::steady_clock::now()) {}

    bool allow() {
        refill();
        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }
        return false;
    }

private:
    void refill() {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_refill_).count();
        tokens_ = std::min(max_tokens_, tokens_ + elapsed * refill_rate_);
        last_refill_ = now;
    }
};

// Usage:
// RateLimiter limiter(10, 5.0);  // burst of 10, sustained 5/sec
// if (limiter.allow()) { process_request(); }
// else { return HTTP_429_TOO_MANY_REQUESTS; }
```

**Design discussion points:**
- **Distributed rate limiting:** Use Redis + Lua script for atomic token operations across multiple servers.
- **Per-user limiting:** `unordered_map<UserId, RateLimiter>` with lazy construction.
- **Thread safety:** Wrap `allow()` with `std::mutex` or use `std::atomic<double>` with CAS loop.
