# Bloomberg — System & Data-Structure Design Questions

> **Focus:** System design and data structure design problems with production-quality C++ implementations, tailored to Bloomberg's financial systems domain.
>
> For pure C++ language mechanics questions, see [bloomberg-cpp.md](./bloomberg-cpp.md).

---

## How to Use This Guide

Each problem follows the structure Bloomberg interviewers expect:

1. **Clarify the requirements** — APIs, constraints, edge cases
2. **Discuss architecture** — data structure choices, trade-offs, rejected alternatives
3. **Analyze complexity** — time, space, amortized bounds
4. **Write production-quality code** — RAII, const-correctness, move semantics, proper naming
5. **Extend** — thread safety, scaling, Bloomberg-specific applications

> [!TIP]
> In a 45-minute Bloomberg interview, spend **15 minutes** on architecture/trade-offs and **25 minutes** on implementation. The remaining 5 minutes for edge cases and extensions. Interviewers want to see you *think* before you code.

---

## Overview

| # | Problem | Domain | Key Concepts |
|---|---|---|---|
| D1 | Underground System | Real-Time Tracking | Running statistics, event pairing, hash map design |
| D2 | Leaderboard | Ranking / Top-K | Dual index (hash + sorted), multiset mechanics |
| D3 | KV Store with TTL & Transactions | Core Infrastructure | Undo-log transactions, TTL expiry strategies |
| D4 | Currency Exchange Graph | Financial Core | BFS on weighted graphs, arbitrage detection |
| D5 | Invalid Transaction Detector | Data Quality | Grouping, time-window proximity queries |
| D6 | Stock Price Spanner | Streaming Data | Monotonic stack, amortized analysis |
| D7 | Order Book (AoS vs SoA) | Trading Engine | Cache-line optimization, SIMD vectorization |
| D8 | VWAP Calculator | Market Data | Sliding window, floating-point stability |
| D9 | All O(1) Data Structure | General | Bucket list + hash map composition |
| D10 | LRU Cache | Caching | Doubly-linked list + hash map, splice |
| D11 | Intrusive Object Pool | Memory Management | Placement new, free-list allocator |
| D12 | Token Bucket Rate Limiter | API Systems | Token bucket algorithm, burst control |
| D13 | Multi-Threaded Limit Order Book | Trading Engine | $O(1)$ limit order matching, intrusive list + hash map |
| D14 | Ticker Stream Sequencer | Market Data | Sequence reordering buffer, sliding deduplication |
| D15 | Real-Time OHLCV Bar Aggregator | Time-Series Data | $O(1)$ tick aggregation into 1m/5m bars |
| D16 | Streaming First Unique Trade Detector | Data Quality | $O(1)$ stream tracking, queue + hash map |
| D17 | Zero-Allocation Event Bus Logger | Low-Latency Systems | Lock-free SPSC telemetry logger |

---

## D1. Underground System

### Problem Statement
Design a system that tracks passenger travel times between stations. Support three operations:
- `checkIn(id, station, time)` — passenger `id` checks in at `station` at time `time`
- `checkOut(id, station, time)` — passenger `id` checks out at `station` at time `time`
- `getAverageTime(start, end)` — return the average travel time from `start` to `end`

All operations must be **O(1) average time**.

### Solution Architecture
```
Map 1: Active check-ins
  passengerId → (stationName, checkInTime)

Map 2: Route statistics
  "stationA→stationB" → (totalTravelTime, tripCount)
```

```cpp
#include <string>
#include <unordered_map>
#include <utility>
#include <shared_mutex>

class UndergroundSystem {
    struct CheckInInfo {
        std::string station;
        int time;
    };
    struct RouteStats {
        double total_time = 0.0;
        int count = 0;
    };

    std::unordered_map<int, CheckInInfo> check_ins_;
    std::unordered_map<std::string, RouteStats> route_stats_;
    mutable std::shared_mutex rw_mtx_;

public:
    void checkIn(int id, std::string stationName, int t) {
        std::unique_lock lock(rw_mtx_);
        check_ins_[id] = {std::move(stationName), t};
    }

    void checkOut(int id, std::string stationName, int t) {
        std::unique_lock lock(rw_mtx_);
        auto it = check_ins_.find(id);
        if (it == check_ins_.end()) return;

        const auto& [start_station, start_time] = it->second;
        std::string route = start_station + "->" + stationName;
        int duration = t - start_time;

        auto& stats = route_stats_[route];
        stats.total_time += duration;
        stats.count += 1;

        check_ins_.erase(it);
    }

    double getAverageTime(const std::string& startStation, const std::string& endStation) const {
        std::shared_lock lock(rw_mtx_); // Thread-safe read lock
        std::string route = startStation + "->" + endStation;
        auto it = route_stats_.find(route);
        if (it == route_stats_.end() || it->second.count == 0) return 0.0;
        return it->second.total_time / it->second.count;
    }
};
```

---

## D2. Leaderboard

### Problem Statement
Design a Leaderboard supporting `addScore(playerId, score)`, `top(K)` returning the sum of top K scores, and `reset(playerId)`.

```cpp
#include <unordered_map>
#include <set>
#include <shared_mutex>

class Leaderboard {
    std::unordered_map<int, int> scores_;
    std::multiset<int, std::greater<int>> sorted_scores_;
    mutable std::shared_mutex mtx_;

public:
    void addScore(int playerId, int score) {
        std::unique_lock lock(mtx_);
        if (scores_.count(playerId)) {
            int old_score = scores_[playerId];
            sorted_scores_.erase(sorted_scores_.find(old_score));
            int new_score = old_score + score;
            scores_[playerId] = new_score;
            sorted_scores_.insert(new_score);
        } else {
            scores_[playerId] = score;
            sorted_scores_.insert(score);
        }
    }

    int top(int K) const {
        std::shared_lock lock(mtx_);
        int sum = 0, count = 0;
        for (int score : sorted_scores_) {
            sum += score;
            if (++count == K) break;
        }
        return sum;
    }

    void reset(int playerId) {
        std::unique_lock lock(mtx_);
        if (scores_.count(playerId)) {
            int old_score = scores_[playerId];
            sorted_scores_.erase(sorted_scores_.find(old_score));
            scores_.erase(playerId);
        }
    }
};
```

---

## D3. KV Store with TTL & Transactions

```cpp
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
#include <chrono>

class TransactionalKV {
    using Clock = std::chrono::steady_clock;
    struct Entry {
        std::string value;
        std::optional<Clock::time_point> expiry;
    };

    std::unordered_map<std::string, Entry> db_;
    std::vector<std::unordered_map<std::string, std::optional<Entry>>> tx_stack_;

public:
    void set(std::string key, std::string val, int ttl_sec = -1) {
        Entry entry;
        entry.value = std::move(val);
        if (ttl_sec > 0) entry.expiry = Clock::now() + std::chrono::seconds(ttl_sec);

        if (!tx_stack_.empty()) {
            auto& active_tx = tx_stack_.back();
            if (!active_tx.count(key)) {
                if (db_.count(key)) active_tx[key] = db_[key];
                else active_tx[key] = std::nullopt;
            }
        }
        db_[key] = std::move(entry);
    }

    std::optional<std::string> get(const std::string& key) {
        auto it = db_.find(key);
        if (it == db_.end()) return std::nullopt;
        if (it->second.expiry.has_value() && Clock::now() >= it->second.expiry.value()) {
            db_.erase(it);
            return std::nullopt;
        }
        return it->second.value;
    }

    void begin() { tx_stack_.push_back({}); }

    void rollback() {
        if (tx_stack_.empty()) return;
        auto tx = std::move(tx_stack_.back());
        tx_stack_.pop_back();
        for (auto& [k, old_val] : tx) {
            if (old_val.has_value()) db_[k] = old_val.value();
            else db_.erase(k);
        }
    }

    void commit() {
        if (!tx_stack_.empty()) tx_stack_.clear();
    }
};
```

---

## D4. Currency Exchange Graph

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

            if (adj_.count(curr)) {
                for (const auto& [next_curr, edge_rate] : adj_.at(curr)) {
                    if (!visited.count(next_curr)) {
                        visited.insert(next_curr);
                        q.push({next_curr, rate * edge_rate});
                    }
                }
            }
        }
        return -1.0;
    }
};
```

---

## D5. Invalid Transaction Detector

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

## D6. Stock Price Spanner

```cpp
#include <stack>
#include <utility>

class StockSpanner {
    std::stack<std::pair<int, int>> st_; // {price, span}
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

---

## D7. Order Book (AoS vs SoA Cache Line Optimization)

```cpp
#include <vector>
#include <cstdint>

// Struct of Arrays (SoA) - Cache-Friendly (16 contiguous uint32_t volumes per 64B cache line)
class OrderBookSoA {
    std::vector<uint64_t> ids_;
    std::vector<double> prices_;
    std::vector<uint32_t> volumes_; // Fits 16 volumes per L1 cache line!
public:
    uint64_t getTotalVolume() const {
        uint64_t total = 0;
        for (uint32_t v : volumes_) total += v;
        return total;
    }
};
```

---

## D8. VWAP Calculator

```cpp
#include <deque>

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

## D9. All O(1) Data Structure

```cpp
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <list>

class AllOne {
    struct Bucket {
        int count;
        std::unordered_set<std::string> keys;
    };

    std::list<Bucket> buckets_;
    std::unordered_map<std::string, std::list<Bucket>::iterator> key_map_;

public:
    void inc(std::string key) {
        if (!key_map_.count(key)) {
            if (buckets_.empty() || buckets_.front().count != 1) {
                buckets_.push_front({1, {}});
            }
            buckets_.front().keys.insert(key);
            key_map_[key] = buckets_.begin();
        } else {
            auto cur_it = key_map_[key];
            auto next_it = std::next(cur_it);
            int new_count = cur_it->count + 1;

            if (next_it == buckets_.end() || next_it->count != new_count) {
                next_it = buckets_.insert(next_it, {new_count, {}});
            }

            next_it->keys.insert(key);
            key_map_[key] = next_it;

            cur_it->keys.erase(key);
            if (cur_it->keys.empty()) buckets_.erase(cur_it);
        }
    }

    void dec(std::string key) {
        if (!key_map_.count(key)) return;
        auto cur_it = key_map_[key];

        if (cur_it->count == 1) {
            key_map_.erase(key);
        } else {
            auto prev_it = std::prev(cur_it);
            int new_count = cur_it->count - 1;

            if (cur_it == buckets_.begin() || prev_it->count != new_count) {
                prev_it = buckets_.insert(cur_it, {new_count, {}});
            }

            prev_it->keys.insert(key);
            key_map_[key] = prev_it;
        }

        cur_it->keys.erase(key);
        if (cur_it->keys.empty()) buckets_.erase(cur_it);
    }

    std::string getMaxKey() const {
        return buckets_.empty() ? "" : *buckets_.back().keys.begin();
    }

    std::string getMinKey() const {
        return buckets_.empty() ? "" : *buckets_.front().keys.begin();
    }
};
```

---

## D10. LRU Cache

```cpp
#include <unordered_map>
#include <list>
#include <utility>

class LRUCache {
    size_t cap_;
    using Pair = std::pair<int, int>;
    std::list<Pair> list_;
    std::unordered_map<int, std::list<Pair>::iterator> map_;

public:
    explicit LRUCache(int capacity) : cap_(capacity) {}

    int get(int key) {
        auto it = map_.find(key);
        if (it == map_.end()) return -1;
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    void put(int key, int value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }
        if (map_.size() >= cap_) {
            int old_key = list_.back().first;
            map_.erase(old_key);
            list_.pop_back();
        }
        list_.push_front({key, value});
        map_[key] = list_.begin();
    }
};
```

---

## D11. Intrusive Object Pool

```cpp
#include <cstddef>
#include <new>

template <typename T, size_t Capacity>
class IntrusiveObjectPool {
    union Node {
        Node* next;
        alignas(alignof(T)) char storage[sizeof(T)];
    };

    Node pool_[Capacity];
    Node* free_head_ = nullptr;

public:
    IntrusiveObjectPool() {
        for (size_t i = 0; i < Capacity; ++i) {
            pool_[i].next = free_head_;
            free_head_ = &pool_[i];
        }
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (!free_head_) throw std::bad_alloc();
        Node* node = free_head_;
        free_head_ = free_head_->next;
        return ::new (static_cast<void*>(node->storage)) T(std::forward<Args>(args)...);
    }

    void deallocate(T* ptr) noexcept {
        if (!ptr) return;
        ptr->~T();
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = free_head_;
        free_head_ = node;
    }
};
```

---

## D12. Token Bucket Rate Limiter

```cpp
#include <chrono>
#include <algorithm>

class RateLimiter {
    using Clock = std::chrono::steady_clock;
    double tokens_;
    double max_tokens_;
    double refill_rate_;
    Clock::time_point last_refill_;

public:
    RateLimiter(double burst_capacity, double sustained_rate)
        : tokens_(burst_capacity), max_tokens_(burst_capacity),
          refill_rate_(sustained_rate), last_refill_(Clock::now()) {}

    [[nodiscard]] bool allow() {
        refill();
        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }
        return false;
    }

private:
    void refill() {
        auto now = Clock::now();
        double elapsed = std::chrono::duration<double>(now - last_refill_).count();
        tokens_ = std::min(max_tokens_, tokens_ + elapsed * refill_rate_);
        last_refill_ = now;
    }
};
```

---

## D13. Multi-Threaded Limit Order Book (L2/L3 Matching Engine)

### Problem Statement
Design a high-frequency Limit Order Book supporting:
- `addOrder(id, side, price, volume)` — Add a limit order with price-time priority
- `cancelOrder(id)` — Cancel an existing order in $O(1)$ time
- `matchOrders()` — Match matching bids and asks at top of book

### Architecture
- **Price Levels**: `std::map<double, std::list<Order>, std::greater<double>>` for Bids (descending).
- **Intrusive Order Index**: `std::unordered_map<uint64_t, OrderLocation>` for $O(1)$ cancellations via `std::list::splice` / `erase`.

```cpp
#include <map>
#include <unordered_map>
#include <list>
#include <cstdint>
#include <shared_mutex>
#include <iostream>

enum class Side { Buy, Sell };

struct Order {
    uint64_t id;
    Side side;
    double price;
    uint32_t volume;
};

class LimitOrderBook {
    using OrderList = std::list<Order>;
    
    // Descending for Bids (highest price first), Ascending for Asks
    std::map<double, OrderList, std::greater<double>> bids_;
    std::map<double, OrderList, std::less<double>> asks_;

    struct Location {
        Side side;
        double price;
        OrderList::iterator it;
    };
    std::unordered_map<uint64_t, Location> order_index_;
    mutable std::shared_mutex mtx_;

public:
    void addOrder(uint64_t id, Side side, double price, uint32_t volume) {
        std::unique_lock lock(mtx_);
        if (side == Side::Buy) {
            auto& level = bids_[price];
            level.push_back({id, side, price, volume});
            order_index_[id] = {side, price, std::prev(level.end())};
        } else {
            auto& level = asks_[price];
            level.push_back({id, side, price, volume});
            order_index_[id] = {side, price, std::prev(level.end())};
        }
        match();
    }

    void cancelOrder(uint64_t id) {
        std::unique_lock lock(mtx_);
        auto it = order_index_.find(id);
        if (it == order_index_.end()) return;

        auto [side, price, list_it] = it->second;
        if (side == Side::Buy) {
            bids_[price].erase(list_it);
            if (bids_[price].empty()) bids_.erase(price);
        } else {
            asks_[price].erase(list_it);
            if (asks_[price].empty()) asks_.erase(price);
        }
        order_index_.erase(it);
    }

private:
    void match() {
        while (!bids_.empty() && !asks_.empty()) {
            auto best_bid_it = bids_.begin();
            auto best_ask_it = asks_.begin();

            if (best_bid_it->first < best_ask_it->first) break; // No match possible

            auto& bid_order = best_bid_it->second.front();
            auto& ask_order = best_ask_it->second.front();

            uint32_t trade_vol = std::min(bid_order.volume, ask_order.volume);
            bid_order.volume -= trade_vol;
            ask_order.volume -= trade_vol;

            if (bid_order.volume == 0) {
                order_index_.erase(bid_order.id);
                best_bid_it->second.pop_front();
                if (best_bid_it->second.empty()) bids_.erase(best_bid_it);
            }
            if (ask_order.volume == 0) {
                order_index_.erase(ask_order.id);
                best_ask_it->second.pop_front();
                if (best_ask_it->second.empty()) asks_.erase(best_ask_it);
            }
        }
    }
};
```
- **Time Complexity:** Add: $O(\log P)$, Cancel: $O(1)$ amortized, Match: $O(1)$ per trade execution.

---

## D14. Distributed Market Data Ticker Stream Sequencer & Deduplicator

### Problem Statement
Reorder incoming out-of-order market data packet streams by sequence number and eliminate duplicate messages over a sliding time window.

```cpp
#include <map>
#include <unordered_set>
#include <vector>
#include <cstdint>

struct MarketPacket {
    uint64_t sequence;
    double price;
    uint32_t volume;
};

class PacketSequencer {
    uint64_t expected_seq_ = 1;
    std::map<uint64_t, MarketPacket> buffer_;
    std::unordered_set<uint64_t> seen_seqs_;

public:
    std::vector<MarketPacket> processPacket(MarketPacket pkt) {
        std::vector<MarketPacket> ordered_out;

        if (pkt.sequence < expected_seq_ || seen_seqs_.count(pkt.sequence)) {
            return ordered_out; // Duplicate or stale packet
        }

        seen_seqs_.insert(pkt.sequence);
        buffer_[pkt.sequence] = pkt;

        while (!buffer_.empty() && buffer_.begin()->first == expected_seq_) {
            ordered_out.push_back(buffer_.begin()->second);
            buffer_.erase(buffer_.begin());
            expected_seq_++;
        }
        return ordered_out;
    }
};
```

---

## D15. Real-Time Time-Series Market Data Aggregator (OHLCV Bars)

### Problem Statement
Aggregate streaming price ticks into 1-minute Open-High-Low-Close-Volume (OHLCV) bars with $O(1)$ tick processing.

```cpp
#include <unordered_map>
#include <string>
#include <algorithm>
#include <iostream>

struct OHLCVBar {
    int start_sec;
    double open;
    double high;
    double low;
    double close;
    uint64_t volume;
};

class OHLCVAggregator {
    std::unordered_map<std::string, OHLCVBar> active_bars_;
    static constexpr int BAR_DURATION_SEC = 60;

public:
    void onTick(const std::string& symbol, double price, uint32_t volume, int timestamp_sec) {
        int bar_start = (timestamp_sec / BAR_DURATION_SEC) * BAR_DURATION_SEC;
        auto it = active_bars_.find(symbol);

        if (it == active_bars_.end() || it->second.start_sec != bar_start) {
            if (it != active_bars_.end()) {
                emitBar(symbol, it->second);
            }
            active_bars_[symbol] = {bar_start, price, price, price, price, volume};
        } else {
            auto& bar = it->second;
            bar.high = std::max(bar.high, price);
            bar.low = std::min(bar.low, price);
            bar.close = price;
            bar.volume += volume;
        }
    }

private:
    void emitBar(const std::string& symbol, const OHLCVBar& bar) {
        // Emit bar to downstream historical DB / Bloomberg Terminal feed
    }
};
```

---

## D16. Streaming First Unique Trade ID / Customer Order Detector

### Problem Statement
Maintain the first non-repeating order ID in a stream of millions of transactions in $O(1)$ time per update.

```cpp
#include <unordered_map>
#include <list>
#include <cstdint>

class FirstUniqueOrderDetector {
    std::list<uint64_t> unique_order_list_;
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> order_map_;

public:
    void addOrder(uint64_t order_id) {
        auto it = order_map_.find(order_id);
        if (it == order_map_.end()) {
            unique_order_list_.push_back(order_id);
            order_map_[order_id] = std::prev(unique_order_list_.end());
        } else if (it->second != unique_order_list_.end()) {
            unique_order_list_.erase(it->second);
            order_map_[order_id] = unique_order_list_.end(); // Mark duplicate
        }
    }

    uint64_t getFirstUnique() const {
        return unique_order_list_.empty() ? 0 : unique_order_list_.front();
    }
};
```

---

## D17. Thread-Safe Ring Buffer Event Bus & Zero-Allocation Logger

### Problem Statement
Write a high-throughput, zero-allocation lock-free telemetry event bus router for market feeds.

```cpp
#include <atomic>
#include <array>
#include <optional>
#include <cstdint>

template <typename Event, size_t Capacity>
class EventBusRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) std::array<Event, Capacity> buffer_;

public:
    bool publish(const Event& event) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (t - h >= Capacity) return false;
        buffer_[t & (Capacity - 1)] = event;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    std::optional<Event> consume() {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        if (h == t) return std::nullopt;
        Event ev = buffer_[h & (Capacity - 1)];
        head_.store(h + 1, std::memory_order_release);
        return ev;
    }
};
```
