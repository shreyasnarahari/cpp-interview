# Bloomberg — Design Questions

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

### Why Bloomberg Asks This

This tests your ability to maintain **running statistics on streaming event pairs** — the same pattern used for tracking trade execution latency between venues, measuring message propagation delay between Bloomberg data centers, or computing average processing time per request type.

### Solution Architecture

**Key Insight:** Each passenger generates an *incomplete event* on checkIn that becomes *complete* on checkOut. We need to pair these events and maintain running aggregates per route.

**Data structures (two hash maps):**

```
Map 1: Active check-ins
  passengerId → (stationName, checkInTime)
  Purpose: O(1) lookup when passenger checks out

Map 2: Route statistics
  "stationA→stationB" → (totalTravelTime, tripCount)
  Purpose: O(1) average calculation without storing individual trips
```

**Why this works:**
- On `checkOut`, we look up the passenger's check-in data (O(1)), compute the duration, and update the running totals for that route — one hash lookup + one addition
- On `getAverageTime`, we divide `totalTime / count` — O(1)
- We don't store individual trip records — constant space per route regardless of passenger volume

**Alternatives considered & rejected:**
- Storing all individual trips: O(1) insert but O(N) average. Wastes memory.
- Using a sorted structure for time-range queries: Over-engineered for this requirement.

**Edge cases:**
- Passenger checks in but never checks out → stays in active map (memory concern for production)
- Passenger checks in twice without checking out → overwrite previous check-in
- Route with zero trips → return 0.0

### Complexity

| Operation | Time | Space |
|---|---|---|
| `checkIn` | O(1) amortized | O(1) per active passenger |
| `checkOut` | O(1) amortized | Route stats: O(1) per distinct route |
| `getAverageTime` | O(1) | — |
| **Total space** | — | **O(P + R)** where P = active passengers, R = distinct routes |

### Implementation

```cpp
#include <string>
#include <unordered_map>
#include <utility>
#include <stdexcept>

class UndergroundSystem {
public:
    UndergroundSystem() = default;

    // Non-copyable (owns stateful maps — copying would be a design error)
    UndergroundSystem(const UndergroundSystem&) = delete;
    UndergroundSystem& operator=(const UndergroundSystem&) = delete;

    void checkIn(int passengerId, std::string station, int time) {
        // Overwrite if passenger checks in again without checking out
        active_checkins_[passengerId] = {std::move(station), time};
    }

    void checkOut(int passengerId, std::string station, int time) {
        auto it = active_checkins_.find(passengerId);
        if (it == active_checkins_.end()) {
            throw std::invalid_argument("Passenger has no active check-in");
        }

        const auto& [start_station, start_time] = it->second;
        std::string route_key = makeRouteKey(start_station, station);
        int duration = time - start_time;

        // Update running aggregates (no individual trip storage)
        auto& [total_time, trip_count] = route_stats_[route_key];
        total_time += duration;
        ++trip_count;

        active_checkins_.erase(it);  // passenger is no longer in transit
    }

    [[nodiscard]] double getAverageTime(const std::string& startStation,
                                        const std::string& endStation) const {
        std::string route_key = makeRouteKey(startStation, endStation);
        auto it = route_stats_.find(route_key);
        if (it == route_stats_.end()) return 0.0;

        const auto& [total_time, trip_count] = it->second;
        return static_cast<double>(total_time) / trip_count;
    }

    // Production API: expose metrics for monitoring dashboards
    [[nodiscard]] size_t activePassengers() const { return active_checkins_.size(); }
    [[nodiscard]] size_t trackedRoutes() const { return route_stats_.size(); }

private:
    struct CheckInData {
        std::string station;
        int time;
    };

    struct RouteAggregate {
        long long total_time = 0;  // use long long to prevent overflow on heavy routes
        int trip_count = 0;
    };

    std::unordered_map<int, CheckInData> active_checkins_;
    std::unordered_map<std::string, RouteAggregate> route_stats_;

    [[nodiscard]] static std::string makeRouteKey(const std::string& from, 
                                                   const std::string& to) {
        // Use a separator that can't appear in station names
        return from + '\x1F' + to;  // ASCII Unit Separator
    }
};
```

**What makes this "strong hire" code:**
- Named structs (`CheckInData`, `RouteAggregate`) instead of raw `pair` — self-documenting
- `[[nodiscard]]` on query functions — prevents ignoring return values
- Non-copyable — prevents accidental expensive map copies
- `long long` for `total_time` — prevents overflow on high-volume routes
- `'\x1F'` separator instead of `"->"` — prevents collision if station names contain `"->"` 
- `const` correctness on all read-only methods
- Exception on invalid checkOut — production systems don't silently swallow errors

### Production Extensions

- **Thread safety:** Use `std::shared_mutex` — multiple threads can call `getAverageTime` concurrently (shared lock), while `checkIn`/`checkOut` take exclusive locks
- **Stale passenger eviction:** Background thread periodically scans `active_checkins_` and removes entries older than a threshold (e.g., 24 hours)
- **Percentile tracking:** Replace `RouteAggregate` with a `tdigest` or `DDSketch` to track p50/p95/p99 travel times

---

## D2. Leaderboard

### Problem Statement

Design a Leaderboard supporting:
- `addScore(playerId, score)` — add `score` to player's total (accumulate)
- `top(K)` — return the sum of the top K player scores
- `reset(playerId)` — remove the player entirely

### Why Bloomberg Asks This

Directly maps to ranked instrument lists — top performers by return, top sectors by volume, most active tickers. Bloomberg Terminal's MOST function shows exactly this: ranked securities by a metric.

### Solution Architecture

**Core challenge:** We need both O(1) player lookup *and* sorted score traversal.

**Solution: Dual-index data structure:**

```
Index 1: unordered_map<playerId, score>   ← O(1) lookup by player
Index 2: multiset<score, greater<>>       ← O(K) traversal for top-K

Both indexes must be kept in sync on every mutation.
```

**Why `multiset` over alternatives:**

| Alternative | `addScore` | `top(K)` | `reset` | Why rejected |
|---|---|---|---|---|
| Sorted `vector` | O(N) | O(K) | O(N) | Linear insert/remove |
| `priority_queue` | O(log N) | O(K log N) | O(N) | **Can't remove arbitrary elements** |
| `multiset` ✅ | O(log N) | O(K) | O(log N) | Supports all operations efficiently |
| Order-statistic tree | O(log N) | O(log N) | O(log N) | Non-standard; over-engineering |

**Critical detail:** When updating a score, we must remove the *old* score from the multiset before inserting the *new* one. We find the exact iterator with `sorted_scores_.find(old_score)` to avoid erasing all entries with that score.

### Complexity

| Operation | Time | Why |
|---|---|---|
| `addScore` | O(log N) | BST erase + insert |
| `top(K)` | O(K) | In-order traversal of first K elements |
| `reset` | O(log N) | BST erase + hash erase |
| **Space** | O(N) | One entry per player in each index |

### Implementation

```cpp
#include <unordered_map>
#include <set>
#include <stdexcept>

class Leaderboard {
public:
    void addScore(int playerId, int score) {
        if (score <= 0) return;  // guard against invalid input

        auto it = player_scores_.find(playerId);
        if (it != player_scores_.end()) {
            // Player exists: remove old score from sorted index, update, re-insert
            removeFromSortedIndex(it->second);
            it->second += score;
            sorted_scores_.insert(it->second);
        } else {
            // New player
            player_scores_[playerId] = score;
            sorted_scores_.insert(score);
        }
    }

    [[nodiscard]] int top(int K) const {
        if (K <= 0) return 0;

        int sum = 0;
        int count = 0;
        for (int s : sorted_scores_) {
            sum += s;
            if (++count >= K) break;
        }
        return sum;
    }

    void reset(int playerId) {
        auto it = player_scores_.find(playerId);
        if (it == player_scores_.end()) return;

        removeFromSortedIndex(it->second);
        player_scores_.erase(it);
    }

    [[nodiscard]] size_t playerCount() const { return player_scores_.size(); }

private:
    std::unordered_map<int, int> player_scores_;               // playerId → total score
    std::multiset<int, std::greater<int>> sorted_scores_;      // scores sorted descending

    void removeFromSortedIndex(int score) {
        // Erase exactly ONE instance of this score (not all duplicates)
        auto sit = sorted_scores_.find(score);
        if (sit != sorted_scores_.end()) {
            sorted_scores_.erase(sit);  // erase by ITERATOR, not by value
        }
    }
};
```

**What makes this "strong hire" code:**
- `removeFromSortedIndex` helper — isolates the critical "erase one, not all" logic with a comment explaining why
- `erase(iterator)` not `erase(value)` — erasing by value removes ALL duplicates, a common bug
- `[[nodiscard]]` on `top` — prevents ignoring the return value
- Input validation on `addScore` and `top`
- Clear separation of the two indexes with named member variables

### Production Extensions

- **Sliding window leaderboard:** "Top K in the last 5 minutes" — combine with a time-based eviction queue. On each `addScore`, also push `(timestamp, playerId, delta)` into a deque. Background thread pops expired entries and subtracts their contributions.
- **Top K in O(1):** Maintain a running `total_sum` of all scores. Track the Kth-largest element and sum-of-top-K separately. Every add/remove only updates if the changed score crosses the K boundary.

---

## D3. In-Memory Key-Value Store with TTL & Transactions

### Problem Statement

Implement an in-memory key-value store with:
- `set(key, value, ttl_seconds)` — store with optional TTL
- `get(key)` → value or null — return null if expired or missing
- `del(key)` — delete a key
- `begin()` — start a new nested transaction
- `commit()` — commit the current transaction
- `rollback()` — undo all changes in the current transaction

### Why Bloomberg Asks This

Tests design of configuration stores, session management, and transactional data layers. Bloomberg's internal systems use transactional key-value stores for real-time configuration propagation across thousands of Terminal instances. The TTL component mirrors session token expiry and ephemeral cache entries.

### Solution Architecture

**Two independent concerns to solve:**

**Concern 1 — TTL Expiry:**
- **Lazy expiry (chosen):** Check if an entry is expired on every `get()`. Stale entries stay in memory until accessed.
  - **Pro:** Zero background overhead, simple implementation
  - **Con:** Memory can grow if keys are set-and-forgotten
- **Eager expiry (alternative):** Use a `priority_queue<expiry_time, key>` + background thread that wakes on the nearest expiry. Used in Redis.
  - **Pro:** Bounded memory
  - **Con:** Background thread complexity, requires coordination

**Concern 2 — Transactions (undo-log pattern):**

```
Transaction Stack (vector of undo-frames):
┌─────────────────────────────────────┐
│ Frame 2 (current)                   │
│   key_a → old_value_of_a            │  ← rollback restores these values
│   key_c → nullopt (didn't exist)    │  ← rollback deletes key_c
├─────────────────────────────────────┤
│ Frame 1 (parent)                    │
│   key_a → original_value_of_a       │
│   key_b → nullopt                   │
└─────────────────────────────────────┘
```

**Undo-log vs command-log:**
- **Undo-log (chosen):** Before each mutation, save the *old* value. Rollback replays old values.
  - Pro: Rollback is a simple restore. `get()` reads the current DB directly.
- **Command-log (alternative):** Record all set/del commands. Rollback discards commands, commit replays onto base.
  - Pro: Cheaper if most transactions commit.
  - Con: `get()` must scan the log — slower.

**Nested transaction semantics:**
- `commit()` at depth > 1: merges current undo-frame into the parent frame (parent can still rollback everything)
- `commit()` at depth 1: discards the undo-frame (changes are permanent)
- `rollback()` at any depth: restores all keys to their pre-transaction state for that depth

### Complexity

| Operation | Time | Notes |
|---|---|---|
| `set` | O(1) amortized | Hash insert + undo-log save |
| `get` | O(1) amortized | Hash lookup + expiry check |
| `del` | O(1) amortized | Hash erase + undo-log save |
| `begin` | O(1) | Push empty frame |
| `rollback` | O(M) | M = mutations in this transaction |
| `commit` (depth 1) | O(1) | Just pop the frame |
| `commit` (depth > 1) | O(M) | Merge M entries into parent frame |

### Implementation

```cpp
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <stdexcept>

class TransactionalKVStore {
public:
    void set(const std::string& key, std::string value, int ttl_seconds = -1) {
        saveUndoEntry(key);

        Entry entry;
        entry.value = std::move(value);
        if (ttl_seconds > 0) {
            entry.expiry = Clock::now() + std::chrono::seconds(ttl_seconds);
        }
        store_[key] = std::move(entry);
    }

    [[nodiscard]] std::optional<std::string> get(const std::string& key) const {
        auto it = store_.find(key);
        if (it == store_.end()) return std::nullopt;
        if (it->second.isExpired()) return std::nullopt;  // lazy expiry
        return it->second.value;
    }

    bool del(const std::string& key) {
        saveUndoEntry(key);
        return store_.erase(key) > 0;
    }

    void begin() {
        undo_stack_.emplace_back();  // push empty transaction frame
    }

    void rollback() {
        if (undo_stack_.empty()) {
            throw std::logic_error("No active transaction to rollback");
        }

        // Restore all keys to their pre-transaction state
        auto frame = std::move(undo_stack_.back());
        undo_stack_.pop_back();

        for (auto& [key, old_entry] : frame) {
            if (old_entry.has_value()) {
                store_[key] = std::move(*old_entry);   // restore old value
            } else {
                store_.erase(key);                      // key didn't exist before
            }
        }
    }

    void commit() {
        if (undo_stack_.empty()) {
            throw std::logic_error("No active transaction to commit");
        }

        if (undo_stack_.size() == 1) {
            // Top-level commit: changes are permanent, discard undo log
            undo_stack_.pop_back();
        } else {
            // Nested commit: merge undo entries into parent frame
            // so parent can still rollback everything
            auto frame = std::move(undo_stack_.back());
            undo_stack_.pop_back();

            auto& parent = undo_stack_.back();
            for (auto& [key, entry] : frame) {
                // Only save the FIRST undo entry per key (the original value)
                if (!parent.count(key)) {
                    parent[key] = std::move(entry);
                }
            }
        }
    }

    [[nodiscard]] bool inTransaction() const { return !undo_stack_.empty(); }
    [[nodiscard]] size_t transactionDepth() const { return undo_stack_.size(); }
    [[nodiscard]] size_t size() const { return store_.size(); }

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct Entry {
        std::string value;
        std::optional<TimePoint> expiry;

        [[nodiscard]] bool isExpired() const {
            return expiry.has_value() && Clock::now() >= *expiry;
        }
    };

    // nullopt in the undo map means "this key did not exist before this transaction"
    using UndoFrame = std::unordered_map<std::string, std::optional<Entry>>;

    std::unordered_map<std::string, Entry> store_;
    std::vector<UndoFrame> undo_stack_;

    void saveUndoEntry(const std::string& key) {
        if (undo_stack_.empty()) return;       // not in a transaction

        auto& frame = undo_stack_.back();
        if (frame.count(key)) return;          // already saved — don't overwrite original

        auto it = store_.find(key);
        if (it != store_.end()) {
            frame[key] = it->second;           // save current value for rollback
        } else {
            frame[key] = std::nullopt;         // key doesn't exist yet
        }
    }
};
```

**What makes this "strong hire" code:**
- Undo-log pattern is clearly commented with *why* each step is necessary
- Nested commit correctly merges with `if (!parent.count(key))` — preserving the *original* pre-parent value
- Exceptions on invalid `commit`/`rollback` — production systems don't silently succeed on logic errors
- Clean type aliases (`Clock`, `TimePoint`, `UndoFrame`) — improves readability
- `isExpired()` as a const member — encapsulates TTL logic cleanly
- Diagnostic methods (`transactionDepth()`, `size()`) — useful for debugging

---

## D4. Currency Exchange Graph

### Problem Statement

Given currency exchange pairs (e.g., `USD → EUR = 0.85`, `EUR → GBP = 0.88`), find the conversion rate between any two currencies. Return -1.0 if no path exists.

### Why Bloomberg Asks This

Direct application to FX trading desks. Bloomberg Terminal users query cross-currency rates constantly. The FX module computes derived rates from known pairs — exactly this algorithm. This also tests graph thinking in a financial context.

### Solution Architecture

**Model:** Model currencies as nodes in a weighted directed graph. Each exchange rate is a directed edge with a multiplicative weight. The reciprocal rate is the inverse edge.

```
Graph:
  USD ──0.85──→ EUR ──0.88──→ GBP
  USD ←──1/0.85── EUR ←──1/0.88── GBP

Query: getRate("USD", "GBP")
BFS path: USD → EUR → GBP
Rate:     1.0 × 0.85 × 0.88 = 0.748
```

**Algorithm: BFS (not DFS, not Dijkstra)**
- **BFS** finds *any* path — all paths between two currencies should yield the same rate if the market is consistent (no arbitrage). BFS finds the shortest path, minimizing floating-point multiplication chain.
- **DFS** would also work but risks stack overflow on large currency graphs.
- **Dijkstra** is irrelevant — we're not minimizing cost; we're computing a product along any path.

**Floating-point precision considerations:**
- Each multiplication introduces ~10⁻¹⁶ relative error (double precision)
- For chains of length L, accumulated error is ~L × 10⁻¹⁶ — acceptable for L < 10
- For longer chains, use **log-space**: `log(rate1) + log(rate2)` → `exp(sum)` converts multiplications to additions (more numerically stable)

**Arbitrage detection (bonus):**
- If any cycle in the graph has a product > 1.0, an arbitrage opportunity exists
- Detection: Bellman-Ford on log-rates — a negative-weight cycle equals arbitrage
- Bloomberg application: compliance systems flag circular FX arbitrage

### Complexity

| Operation | Time | Space |
|---|---|---|
| `addRate` | O(1) | O(1) per pair |
| `getRate` | O(V + E) | O(V) for BFS visited set |
| **Total space** | — | O(V + E) adjacency list |

### Implementation

```cpp
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <stdexcept>

class CurrencyConverter {
public:
    void addRate(const std::string& from, const std::string& to, double rate) {
        if (rate <= 0.0) {
            throw std::invalid_argument("Exchange rate must be positive");
        }
        // Bidirectional: A→B at rate R implies B→A at rate 1/R
        adjacency_[from].push_back({to, rate});
        adjacency_[to].push_back({from, 1.0 / rate});
    }

    [[nodiscard]] double getRate(const std::string& from, const std::string& to) const {
        // Edge cases
        if (!adjacency_.count(from) || !adjacency_.count(to)) return -1.0;
        if (from == to) return 1.0;

        // BFS: accumulate multiplicative rate along the path
        std::queue<std::pair<std::string, double>> frontier;  // {currency, accumulated_rate}
        std::unordered_set<std::string> visited;

        frontier.push({from, 1.0});
        visited.insert(from);

        while (!frontier.empty()) {
            auto [current_currency, current_rate] = frontier.front();
            frontier.pop();

            // Check all neighbors
            for (const auto& [neighbor, edge_rate] : adjacency_.at(current_currency)) {
                if (neighbor == to) {
                    return current_rate * edge_rate;  // found target — return product
                }
                if (!visited.count(neighbor)) {
                    visited.insert(neighbor);
                    frontier.push({neighbor, current_rate * edge_rate});
                }
            }
        }

        return -1.0;  // no path exists
    }

    [[nodiscard]] size_t currencyCount() const { return adjacency_.size(); }

private:
    struct Edge {
        std::string target;
        double rate;
    };

    std::unordered_map<std::string, std::vector<Edge>> adjacency_;
};
```

**What makes this "strong hire" code:**
- Named `Edge` struct instead of `pair<string, double>` — clear intent
- Early return when `neighbor == to` inside the BFS loop — avoids unnecessary queue operations
- Input validation on `addRate`
- `const` correctness on `getRate` 
- Handles self-query (`from == to`) and missing currency edge cases explicitly

---

## D5. Invalid Transaction Detector

### Problem Statement

Given a list of transaction strings `"name,time,amount,city"`, flag transactions that are invalid. A transaction is invalid if:
1. `amount > 1000`, **OR**
2. It occurs within 60 minutes of another transaction **with the same name in a different city**

### Why Bloomberg Asks This

Compliance systems at Bloomberg flag suspicious trading patterns using similar time-proximity rules: "flag if the same account traded the same instrument on two different exchanges within T minutes."

### Solution Architecture

**Key insight:** Rule 2 requires comparing every transaction against others *with the same name*. Grouping by name first reduces comparisons from O(N²) to O(N × M) where M = max transactions per name.

**Approach:**
1. Parse all transactions into structured objects
2. Group by `name` using a hash map
3. For each transaction: check Rule 1 (amount > 1000), then check Rule 2 by scanning same-name transactions for time proximity + different city
4. Mark **both** transactions as invalid when Rule 2 triggers

**Optimization for large inputs:** Sort each name group by time, then use binary search (`lower_bound` / `upper_bound`) to find transactions within the 60-minute window — reduces from O(M²) to O(M log M) per name.

### Complexity

| Approach | Time | Space |
|---|---|---|
| Brute force per name group | O(N × M) where M = max group size | O(N) |
| Sorted + binary search | O(N log M) | O(N) |

### Implementation

```cpp
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>

class TransactionValidator {
public:
    struct Transaction {
        std::string name;
        int time;
        int amount;
        std::string city;
        std::string raw;     // original string for output
        int index;           // position in input array
    };

    [[nodiscard]] static std::vector<std::string> findInvalid(
            const std::vector<std::string>& transactions) {
        // Phase 1: Parse and group by name
        std::vector<Transaction> all;
        all.reserve(transactions.size());

        std::unordered_map<std::string, std::vector<size_t>> name_groups;

        for (int i = 0; i < static_cast<int>(transactions.size()); ++i) {
            all.push_back(parse(transactions[i], i));
            name_groups[all.back().name].push_back(i);
        }

        // Phase 2: Flag invalid transactions
        std::vector<bool> invalid(transactions.size(), false);

        for (size_t i = 0; i < all.size(); ++i) {
            const auto& tx = all[i];

            // Rule 1: Amount exceeds threshold
            if (tx.amount > 1000) {
                invalid[i] = true;
            }

            // Rule 2: Same name, different city, within 60 minutes
            for (size_t j : name_groups[tx.name]) {
                if (i == j) continue;
                const auto& other = all[j];
                if (tx.city != other.city && std::abs(tx.time - other.time) <= 60) {
                    invalid[i] = true;
                    invalid[j] = true;  // both directions are invalid
                }
            }
        }

        // Phase 3: Collect results
        std::vector<std::string> result;
        for (size_t i = 0; i < transactions.size(); ++i) {
            if (invalid[i]) {
                result.push_back(transactions[i]);
            }
        }
        return result;
    }

private:
    [[nodiscard]] static Transaction parse(const std::string& raw, int index) {
        Transaction tx;
        tx.raw = raw;
        tx.index = index;

        std::istringstream ss(raw);
        std::string token;

        std::getline(ss, tx.name, ',');
        std::getline(ss, token, ',');  tx.time = std::stoi(token);
        std::getline(ss, token, ',');  tx.amount = std::stoi(token);
        std::getline(ss, tx.city, ',');

        return tx;
    }
};
```

---

## D6. Stock Price Spanner (Monotonic Stack)

### Problem Statement

Design a class that collects daily price quotes and returns the **span** of the current day's price — the number of consecutive previous days (including today) where the price was ≤ today's price.

### Why Bloomberg Asks This

Monotonic stacks are fundamental to streaming financial analytics: computing support/resistance levels, consecutive-day streaks, and next-greater-element queries on price feeds. This pattern appears in Bloomberg's charting engine.

### Solution Architecture

**Brute force:** For each new price, scan backward counting consecutive days with price ≤ current. O(N) per query, O(N²) total.

**Monotonic stack insight:** When a new price arrives that is ≥ the stack top, the stack top will **never be relevant again** — any future query that reaches this price will also pass through the current (higher) price. So we can **absorb** (pop) all dominated entries and accumulate their spans.

```
Stack state trace:
next(100): push (100,1)          stack: [(100,1)]                → return 1
next(80):  push (80,1)           stack: [(100,1), (80,1)]        → return 1
next(60):  push (60,1)           stack: [(100,1), (80,1), (60,1)]→ return 1
next(70):  pop (60,1) → span=2  stack: [(100,1), (80,1), (70,2)]→ return 2
            (70 ≥ 60, absorb its span)
next(60):  push (60,1)           stack: [(100,1),(80,1),(70,2),(60,1)] → return 1
next(75):  pop (60,1) → span=2  
           pop (70,2) → span=4  stack: [(100,1), (80,1), (75,4)]→ return 4
            (75 ≥ 60 and 75 ≥ 70, absorb both spans)
```

**Key invariant:** The stack is always **strictly decreasing** from bottom to top. This is a monotonic (decreasing) stack.

**Amortized O(1) proof:** Each element is pushed onto the stack exactly once and popped at most once. Over N calls to `next()`, total pushes + pops ≤ 2N. Therefore amortized cost per `next()` is O(1).

### Implementation

```cpp
#include <stack>

class StockSpanner {
public:
    [[nodiscard]] int next(int price) {
        int span = 1;

        // Absorb all previous entries with price ≤ current price
        // Their span is "inherited" — they'll never be queried again
        while (!stack_.empty() && stack_.top().price <= price) {
            span += stack_.top().span;
            stack_.pop();
        }

        stack_.push({price, span});
        return span;
    }

private:
    struct Entry {
        int price;
        int span;  // includes all previously absorbed entries
    };

    std::stack<Entry> stack_;
};
```

**What makes this "strong hire" code:**
- Named `Entry` struct with clear fields
- Single-responsibility: the `next` function does one thing with clear logic flow
- The comment "they'll never be queried again" explains *why* we can pop — this is the insight interviewers want to hear

---

## D7. Order Book — AoS vs SoA Optimization

### Problem Statement

Design an order book data structure for aggregating total volume. Compare Array of Structs (AoS) vs Struct of Arrays (SoA) layouts and explain when each is appropriate.

### Why Bloomberg Asks This

Bloomberg's trading engine processes millions of order book updates per second. Cache efficiency is the difference between 1µs and 10µs latency. This question tests whether you understand **hardware-level performance** — a critical skill for financial systems engineering.

### Solution Architecture

**The fundamental tension:** CPUs don't load individual bytes — they load entire **64-byte cache lines**. Your data layout determines how much of each cache line contains useful data vs wasted padding.

**AoS (Array of Structs) — record-oriented:**
```
Memory layout:
[id₁|price₁|vol₁|side₁|ts₁] [id₂|price₂|vol₂|side₂|ts₂] [id₃|price₃|vol₃|side₃|ts₃]
 ←────── 32 bytes ─────────→
                              ← 64-byte cache line loads 2 orders →

When summing volumes: load 32 bytes per order, use only 4 → 12.5% cache utilization
```

**SoA (Struct of Arrays) — column-oriented:**
```
Memory layout:
ids:    [id₁] [id₂] [id₃] [id₄] ...
prices: [p₁]  [p₂]  [p₃]  [p₄]  ...
volumes:[v₁]  [v₂]  [v₃]  [v₄]  [v₅]  [v₆]  [v₇]  [v₈]  [v₉] [v₁₀] [v₁₁] [v₁₂] [v₁₃] [v₁₄] [v₁₅] [v₁₆]
         ←──────────────────── 64-byte cache line loads 16 uint32_t volumes ────────────────────────────────→

When summing volumes: 100% cache utilization + compiler auto-vectorizes to SIMD
```

**Decision framework:**

| Access Pattern | Best Layout | Reasoning |
|---|---|---|
| Scan one column (sum volumes, filter prices) | **SoA** | Only relevant data in cache lines |
| Access all fields of one record | **AoS** | All fields in one cache line |
| Serialize/deserialize records | **AoS** | Contiguous record = memcpy |
| SIMD-friendly bulk operations | **SoA** | Homogeneous arrays enable vectorization |

### Implementation

```cpp
#include <vector>
#include <cstdint>
#include <numeric>
#include <stdexcept>

// ──────────────────────────────────────────────────────────────
// AoS: Best for single-record access patterns
// ──────────────────────────────────────────────────────────────
struct OrderAoS {
    uint64_t id;          // 8 bytes
    double price;         // 8 bytes
    uint32_t volume;      // 4 bytes
    char side;            // 1 byte + 3 bytes padding
    uint64_t timestamp;   // 8 bytes
};
// sizeof(OrderAoS) = 32 bytes → 2 orders per cache line
static_assert(sizeof(OrderAoS) == 32);

// ──────────────────────────────────────────────────────────────
// SoA: Best for column-oriented analytics (volume sums, VWAP, filtering)
// ──────────────────────────────────────────────────────────────
class OrderBookSoA {
public:
    void addOrder(uint64_t id, double price, uint32_t volume, 
                  char side, uint64_t timestamp) {
        ids_.push_back(id);
        prices_.push_back(price);
        volumes_.push_back(volume);
        sides_.push_back(side);
        timestamps_.push_back(timestamp);
    }

    // 16 uint32_t volumes per cache line → 100% utilization
    // Compiler auto-vectorizes: AVX2 processes 8 uint32_t per cycle
    [[nodiscard]] uint64_t totalVolume() const {
        return std::accumulate(volumes_.begin(), volumes_.end(), uint64_t{0});
    }

    // Two arrays accessed in lockstep — still 2x better than AoS for this operation
    [[nodiscard]] double vwap() const {
        if (volumes_.empty()) return 0.0;

        double sum_pv = 0.0;
        uint64_t sum_v = 0;
        for (size_t i = 0; i < prices_.size(); ++i) {
            sum_pv += prices_[i] * volumes_[i];
            sum_v += volumes_[i];
        }
        return sum_v > 0 ? sum_pv / static_cast<double>(sum_v) : 0.0;
    }

    // Column scan: only touch prices array → maximum cache efficiency
    [[nodiscard]] size_t countAbovePrice(double threshold) const {
        size_t count = 0;
        for (double p : prices_) {
            count += (p > threshold);  // branchless increment
        }
        return count;
    }

    [[nodiscard]] size_t orderCount() const { return ids_.size(); }

private:
    std::vector<uint64_t> ids_;
    std::vector<double> prices_;
    std::vector<uint32_t> volumes_;    // contiguous for fast aggregation
    std::vector<char> sides_;
    std::vector<uint64_t> timestamps_;
};
```

---

## D8. Real-Time VWAP Calculator (Sliding Window)

### Problem Statement

Compute trailing 5-minute Volume-Weighted Average Price (VWAP) for streaming stock price ticks:

$$\text{VWAP} = \frac{\sum_{i \in \text{window}} \text{price}_i \times \text{volume}_i}{\sum_{i \in \text{window}} \text{volume}_i}$$

All operations must be **O(1) amortized**.

### Why Bloomberg Asks This

VWAP is the single most important trading benchmark. Institutional traders compare their execution price against VWAP to measure trading quality. Bloomberg Terminal computes VWAP in real-time for every listed security worldwide, processing millions of ticks per second.

### Solution Architecture

**Key insight:** Maintain **running sums** (`Σ(price×volume)` and `Σ(volume)`) instead of recomputing from the window contents. When a tick enters the window, add to the sums. When a tick exits (too old), subtract from the sums.

**Data structure: `deque` as a sliding window**

```
Time axis:  [────────────── 5 minutes ──────────────]
Deque:       tick₁  tick₂  tick₃  ...  tick_n   tick_new
             ↑ front (oldest)                    ↑ back (newest)
             Evict if timestamp_new - timestamp₁ > 300

Running sums updated incrementally:
  On addTick:  sum_pv += price × volume,  sum_v += volume
  On evict:    sum_pv -= evicted.price × evicted.volume,  sum_v -= evicted.volume
```

**Why `deque` and not `queue` or `vector`:**
- Need O(1) `push_back` (new ticks) and O(1) `pop_front` (eviction)
- `std::deque` provides both. `std::queue` wraps deque but hides the front element. `vector` has O(N) `pop_front`.

**Floating-point stability concern:**
- After millions of add/subtract cycles, floating-point drift accumulates
- Production fix: periodically recompute sums from scratch (e.g., every 10,000 ticks)
- Alternative: use Kahan compensated summation for the running sums

### Complexity

| Operation | Time | Amortized Reasoning |
|---|---|---|
| `addTick` | O(1) amortized | Each tick is pushed once and popped once over its lifetime |
| `getVWAP` | O(1) | Division of two running sums |
| **Space** | O(W) | W = ticks within the time window |

### Implementation

```cpp
#include <deque>
#include <cstdint>
#include <chrono>

class VWAPCalculator {
public:
    explicit VWAPCalculator(int window_seconds = 300)
        : window_seconds_(window_seconds) {}

    void addTick(double price, uint32_t volume, int timestamp_sec) {
        // Add new tick to the window
        window_.push_back({price, volume, timestamp_sec});
        sum_price_volume_ += price * volume;
        sum_volume_ += volume;

        // Evict ticks that have fallen outside the window
        evictExpired(timestamp_sec);

        // Periodic recomputation to combat floating-point drift
        if (++tick_count_ % kRecomputeInterval == 0) {
            recomputeSums();
        }
    }

    [[nodiscard]] double getVWAP() const {
        if (sum_volume_ == 0) return 0.0;
        return sum_price_volume_ / static_cast<double>(sum_volume_);
    }

    [[nodiscard]] size_t windowSize() const { return window_.size(); }
    [[nodiscard]] uint64_t totalVolume() const { return sum_volume_; }

private:
    struct Tick {
        double price;
        uint32_t volume;
        int timestamp_sec;
    };

    std::deque<Tick> window_;
    double sum_price_volume_ = 0.0;     // Σ(price × volume) — running sum
    uint64_t sum_volume_ = 0;           // Σ(volume) — running sum
    int window_seconds_;
    uint64_t tick_count_ = 0;

    static constexpr uint64_t kRecomputeInterval = 10'000;

    void evictExpired(int current_time) {
        while (!window_.empty() &&
               (current_time - window_.front().timestamp_sec > window_seconds_)) {
            const auto& expired = window_.front();
            sum_price_volume_ -= expired.price * expired.volume;
            sum_volume_ -= expired.volume;
            window_.pop_front();
        }
    }

    void recomputeSums() {
        sum_price_volume_ = 0.0;
        sum_volume_ = 0;
        for (const auto& tick : window_) {
            sum_price_volume_ += tick.price * tick.volume;
            sum_volume_ += tick.volume;
        }
    }
};
```

**What makes this "strong hire" code:**
- `recomputeSums()` for floating-point drift — shows awareness of numerical stability
- Digit separator (`10'000`) for readability
- Configurable window via constructor (not hardcoded)
- `const` and `[[nodiscard]]` throughout

---

## D9. All O(1) Data Structure

### Problem Statement

Design a data structure with:
- `inc(key)` — increment key's count by 1 (insert with count 1 if new)
- `dec(key)` — decrement key's count by 1 (remove if count reaches 0)
- `getMaxKey()` — return any key with the highest count
- `getMinKey()` — return any key with the lowest count

**All operations must be O(1).**

### Why Bloomberg Asks This

Tests advanced data structure composition. Maps to real-time "most active ticker" tracking — which symbol had the most trades in the last window? The same structure appears in LFU cache implementations.

### Solution Architecture

**Why hash map alone fails:** A hash map gives O(1) `inc`/`dec` but O(N) `getMax`/`getMin` — you'd need to scan all entries.

**Why sorted structure alone fails:** A BST gives O(log N) everything, but we need O(1).

**Solution: Bucket list + hash map composition**

```
Doubly-linked list of COUNT BUCKETS (sorted by count):
HEAD ↔ [count=1: {a,b}] ↔ [count=3: {c}] ↔ [count=5: {d,e}] ↔ TAIL
        ↑ min keys                            ↑ max keys

Hash map for O(1) key → bucket lookup:
  "a" → iterator to [count=1] bucket
  "c" → iterator to [count=3] bucket
  "d" → iterator to [count=5] bucket
```

**On `inc("a")`:**
1. Look up `"a"` in hash map → bucket [count=1]
2. Target count = 2. Check if next bucket has count=2. If not, insert new bucket.
3. Move `"a"` from [count=1] to [count=2]
4. If [count=1] bucket is now empty, remove it from the list

**Why this is O(1):**
- Hash map lookup: O(1)
- `std::list` insert/erase: O(1) by iterator
- Next/prev bucket check: O(1) — just look at adjacent list node
- Buckets only exist for counts that have keys — no gaps, no scanning

### Implementation

```cpp
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <string>

class AllOne {
public:
    void inc(const std::string& key) {
        if (!key_to_bucket_.count(key)) {
            // Brand new key — ensure count=1 bucket exists at front
            if (buckets_.empty() || buckets_.front().count != 1) {
                buckets_.push_front({1, {}});
            }
            buckets_.front().keys.insert(key);
            key_to_bucket_[key] = buckets_.begin();
        } else {
            moveKey(key, +1);
        }
    }

    void dec(const std::string& key) {
        if (!key_to_bucket_.count(key)) return;

        auto curr_bucket = key_to_bucket_[key];
        if (curr_bucket->count == 1) {
            // Count drops to 0 → remove key entirely
            curr_bucket->keys.erase(key);
            key_to_bucket_.erase(key);
            if (curr_bucket->keys.empty()) buckets_.erase(curr_bucket);
        } else {
            moveKey(key, -1);
        }
    }

    [[nodiscard]] std::string getMaxKey() const {
        return buckets_.empty() ? "" : *buckets_.back().keys.begin();
    }

    [[nodiscard]] std::string getMinKey() const {
        return buckets_.empty() ? "" : *buckets_.front().keys.begin();
    }

private:
    struct Bucket {
        int count;
        std::unordered_set<std::string> keys;
    };

    std::list<Bucket> buckets_;  // invariant: sorted ascending by count, no gaps
    std::unordered_map<std::string, std::list<Bucket>::iterator> key_to_bucket_;

    // Move a key from its current bucket to the adjacent bucket with count ± 1.
    // Creates the target bucket if it doesn't exist; removes the source if empty.
    void moveKey(const std::string& key, int direction) {
        auto curr = key_to_bucket_[key];
        int new_count = curr->count + direction;

        // Find or create the target bucket
        std::list<Bucket>::iterator target;
        if (direction == +1) {
            auto next = std::next(curr);
            if (next == buckets_.end() || next->count != new_count) {
                target = buckets_.insert(next, {new_count, {}});
            } else {
                target = next;
            }
        } else {
            if (curr == buckets_.begin()) {
                target = buckets_.insert(curr, {new_count, {}});
            } else {
                auto prev = std::prev(curr);
                if (prev->count != new_count) {
                    target = buckets_.insert(curr, {new_count, {}});
                } else {
                    target = prev;
                }
            }
        }

        // Transfer the key
        target->keys.insert(key);
        key_to_bucket_[key] = target;

        // Clean up source bucket
        curr->keys.erase(key);
        if (curr->keys.empty()) {
            buckets_.erase(curr);
        }
    }
};
```

**What makes this "strong hire" code:**
- `moveKey` helper consolidates the inc/dec logic — avoids duplicating bucket creation/cleanup code
- Clear invariant comment: "sorted ascending by count, no gaps"
- Edge case for `dec` when count drops to 0 handled separately with early return

---

## D10. LRU Cache

### Problem Statement

Design a Least Recently Used (LRU) cache with:
- `get(key)` → value or -1 — marks key as most recently used
- `put(key, value)` — inserts/updates. If at capacity, evict the **least recently used** entry.

Both operations must be **O(1)**.

### Why Bloomberg Asks This

Caching is ubiquitous at Bloomberg — market data snapshots, user session state, instrument metadata, computed analytics. The LRU policy is the default eviction strategy because it captures temporal locality: recently accessed data is likely to be accessed again.

### Solution Architecture

**Data structure: Doubly-linked list + hash map**

```
Hash Map:                     Doubly-Linked List (recency order):
key → list iterator           HEAD (most recent) ↔ [K₃,V₃] ↔ [K₁,V₁] ↔ [K₅,V₅] ↔ TAIL (least recent)

On get(K₁): splice K₁'s node to HEAD → O(1)
On put(K_new) at capacity: pop TAIL → O(1), erase from map → O(1), push_front → O(1)
```

**Why `std::list::splice` is the key insight:**
- `splice(pos, list, it)` moves a node within the list **without copying or allocating** — it just re-links pointers
- This gives O(1) "move to front" — the critical operation for LRU
- Without `splice`, you'd need to erase + re-insert = 2 allocations

**Why not other structures:**
- `std::deque`: O(1) front/back but O(N) to remove from middle
- `std::unordered_map` alone: no ordering information
- `std::map` with timestamp keys: O(log N) operations

### Implementation

```cpp
#include <unordered_map>
#include <list>
#include <stdexcept>

class LRUCache {
public:
    explicit LRUCache(int capacity) : capacity_(capacity) {
        if (capacity <= 0) throw std::invalid_argument("Capacity must be positive");
    }

    [[nodiscard]] int get(int key) {
        auto it = index_.find(key);
        if (it == index_.end()) return -1;

        // Move to front = mark as most recently used
        // splice is O(1) — just pointer relinking, no copy/allocation
        order_.splice(order_.begin(), order_, it->second);
        return it->second->value;
    }

    void put(int key, int value) {
        auto it = index_.find(key);

        if (it != index_.end()) {
            // Key exists: update value and move to front
            it->second->value = value;
            order_.splice(order_.begin(), order_, it->second);
            return;
        }

        // Key is new — check capacity
        if (static_cast<int>(index_.size()) >= capacity_) {
            // Evict least recently used (back of list)
            const auto& victim = order_.back();
            index_.erase(victim.key);
            order_.pop_back();
        }

        // Insert new entry at front (most recent)
        order_.push_front({key, value});
        index_[key] = order_.begin();
    }

    [[nodiscard]] size_t size() const { return index_.size(); }
    [[nodiscard]] int capacity() const { return capacity_; }

private:
    struct CacheEntry {
        int key;    // stored here so we can find the hash map entry during eviction
        int value;
    };

    int capacity_;
    std::list<CacheEntry> order_;        // front = most recent, back = least recent
    std::unordered_map<int, std::list<CacheEntry>::iterator> index_;
};
```

**What makes this "strong hire" code:**
- `CacheEntry` struct with named fields — `order_.back().key` reads better than `order_.back().first`
- Key stored inside list entry — enables O(1) eviction (can find hash map entry from list entry)
- Comment on `splice` explaining *why* it's O(1)
- Input validation on capacity

### Production Extensions

- **Thread-safe LRU:** Use `std::shared_mutex`. `get` takes exclusive lock (modifies recency order). For true concurrent reads, implement a lock-free variant with sharded LRU maps.
- **TTL-aware LRU:** Add `expiry_time` to each entry. On `get`, check if expired before returning. Background thread evicts expired entries periodically.
- **LRU with frequency (LFU):** Combine with the All O(1) structure (D9) — evict the least-frequently-used key among those with the lowest recency.

---

## D11. Intrusive Object Pool

### Problem Statement

Design a fixed-size object pool that provides O(1) allocate and O(1) deallocate with **zero heap allocations** after initialization. All memory must be pre-allocated.

### Why Bloomberg Asks This

Low-latency trading systems pre-allocate all memory at startup. `malloc` is non-deterministic (can take 100ns–10µs depending on heap fragmentation). A pre-allocated pool guarantees deterministic allocation time — critical for sub-microsecond order entry paths.

### Solution Architecture

**Core idea: Intrusive free list**

Pre-allocate an array of N slots. When a slot is free, use its own memory to store a `next` pointer, forming a singly-linked free list. On allocate, pop from the free list. On deallocate, push back.

```
Initial state (all free):
free_head_ → [slot₃] → [slot₂] → [slot₁] → [slot₀] → nullptr
              ↑ next     ↑ next     ↑ next     ↑ next

After allocate() — returns slot₃:
free_head_ → [slot₂] → [slot₁] → [slot₀] → nullptr
              slot₃ now holds a live T object (via placement new)

After deallocate(slot₃) — returns slot₃ to free list:
free_head_ → [slot₃] → [slot₂] → [slot₁] → [slot₀] → nullptr
              slot₃'s memory reused for the next pointer
```

**Why "intrusive"?**
- The `next` pointer is stored **inside the slot's own memory** (via a `union`)
- No external free-list node allocation needed
- Zero additional memory overhead per slot

**Key C++ concepts tested:**
- `union` for type punning (slot is either a live object or a free-list node)
- Placement `new` for constructing objects at a specific address
- Explicit destructor calls (`ptr->~T()`) on deallocation
- `alignas` for proper memory alignment
- Variadic templates for forwarding constructor arguments

### Implementation

```cpp
#include <cstddef>
#include <new>
#include <cassert>
#include <utility>
#include <type_traits>

template <typename T, size_t Capacity>
class ObjectPool {
    static_assert(Capacity > 0, "Pool capacity must be positive");
    static_assert(sizeof(T) >= sizeof(void*), 
                  "Object must be large enough to hold a free-list pointer");

public:
    ObjectPool() noexcept {
        // Build the free list by linking all slots in reverse order
        // so that allocations proceed forward through the array (cache-friendly)
        free_head_ = nullptr;
        for (size_t i = 0; i < Capacity; ++i) {
            auto* slot = reinterpret_cast<FreeNode*>(&storage_[i]);
            slot->next = free_head_;
            free_head_ = slot;
        }
    }

    ~ObjectPool() = default;  // pool does NOT destroy live objects — caller's responsibility

    // Non-copyable, non-movable (addresses of pooled objects must remain stable)
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Allocate a slot and construct a T in-place with forwarded arguments
    template <typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) {
        assert(free_head_ != nullptr && "Object pool exhausted");

        // Pop from free list
        FreeNode* node = free_head_;
        free_head_ = node->next;
        ++live_count_;

        // Construct T at the slot's address via placement new
        return ::new (static_cast<void*>(node)) T(std::forward<Args>(args)...);
    }

    // Destroy the object and return the slot to the free list
    void deallocate(T* ptr) noexcept {
        assert(ptr != nullptr && "Cannot deallocate nullptr");
        assert(owns(ptr) && "Pointer does not belong to this pool");

        ptr->~T();  // explicit destructor call (paired with placement new)

        // Push slot back onto free list
        auto* node = reinterpret_cast<FreeNode*>(ptr);
        node->next = free_head_;
        free_head_ = node;
        --live_count_;
    }

    // Check if a pointer belongs to this pool's memory range
    [[nodiscard]] bool owns(const T* ptr) const noexcept {
        const auto* raw = reinterpret_cast<const char*>(ptr);
        const auto* begin = reinterpret_cast<const char*>(&storage_[0]);
        const auto* end = reinterpret_cast<const char*>(&storage_[Capacity]);
        return raw >= begin && raw < end;
    }

    [[nodiscard]] size_t liveCount() const noexcept { return live_count_; }
    [[nodiscard]] size_t freeCount() const noexcept { return Capacity - live_count_; }
    [[nodiscard]] static constexpr size_t capacity() { return Capacity; }

private:
    struct FreeNode {
        FreeNode* next;
    };

    // Storage: properly aligned raw memory for T objects
    alignas(alignof(T)) std::aligned_storage_t<sizeof(T), alignof(T)> storage_[Capacity];
    FreeNode* free_head_ = nullptr;
    size_t live_count_ = 0;
};

// Usage example:
// struct Order { uint64_t id; double price; uint32_t volume; };
// ObjectPool<Order, 100'000> order_pool;
// Order* o = order_pool.allocate(42, 150.25, 100);
// order_pool.deallocate(o);
```

**What makes this "strong hire" code:**
- `static_assert` on object size — catches the edge case where T is smaller than a pointer (free-list node won't fit)
- `owns()` validation in `deallocate` — prevents use-after-free from wrong pool
- `live_count_` tracking — production diagnostic
- `std::aligned_storage_t` for proper alignment — avoids UB from misaligned access
- Non-copyable with a comment explaining *why* (address stability)
- `noexcept` on `deallocate` — destruction and pointer manipulation can't throw
- Explicit `::new` (global placement new) — prevents user-defined `operator new` from being called

---

## D12. Token Bucket Rate Limiter

### Problem Statement

Implement a rate limiter that controls request throughput. Support:
- **Sustained rate:** Maximum N requests per second averaged over time
- **Burst capacity:** Allow brief bursts of up to M requests
- `allow()` → `true` if the request is permitted, `false` if throttled

### Why Bloomberg Asks This

Bloomberg Terminal data feeds serve millions of subscribers. API rate limiting prevents:
- Individual clients from overwhelming shared infrastructure
- Cascading failures during market volatility (everyone refreshes simultaneously)
- Abuse of real-time market data endpoints

### Solution Architecture

**Algorithm: Token Bucket**

```
Bucket:  [●●●●●●●○○○]   max_tokens = 10, current = 7
          ↑ tokens

On allow():
  1. Refill tokens based on elapsed time:  tokens += elapsed × rate
  2. Cap at max:  tokens = min(tokens, max_tokens)
  3. If tokens ≥ 1: consume one token, return true
  4. Else: return false (throttled)
```

**Why token bucket over alternatives:**

| Algorithm | Burst handling | Smoothing | Implementation |
|---|---|---|---|
| **Token Bucket** ✅ | Configurable burst size | Smooth over time | Simple, O(1) |
| Fixed Window | Edge bursts (2N at boundary) | Coarse | Simple |
| Sliding Window Log | No bursts | Perfect | O(log N) per request |
| Leaky Bucket | No bursts | Perfect | Needs timer/queue |

**Two parameters control behavior:**
- `max_tokens` (burst capacity): how many requests can happen instantly after idle period
- `refill_rate` (sustained rate): tokens added per second during continuous usage

Example: `max_tokens=10, refill_rate=5.0` → burst of 10 requests immediately, then sustained 5/sec.

**Key implementation detail: Lazy refill**
- Don't use a background timer to add tokens. Instead, on each `allow()` call, compute how much time has passed since the last call and add `elapsed × refill_rate` tokens.
- This makes the implementation stateless with respect to time — no threads, no timers.

### Implementation

```cpp
#include <chrono>
#include <algorithm>

class RateLimiter {
public:
    // burst_capacity: maximum tokens stored (burst allowance)
    // sustained_rate: tokens added per second
    RateLimiter(double burst_capacity, double sustained_rate)
        : tokens_(burst_capacity)
        , max_tokens_(burst_capacity)
        , refill_rate_(sustained_rate)
        , last_refill_(Clock::now()) {}

    // Returns true if the request is allowed, false if throttled
    [[nodiscard]] bool allow() {
        refill();

        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }
        return false;  // throttled
    }

    // Variant: try to consume N tokens at once (for batch operations)
    [[nodiscard]] bool allowN(int n) {
        if (n <= 0) return true;
        refill();

        double required = static_cast<double>(n);
        if (tokens_ >= required) {
            tokens_ -= required;
            return true;
        }
        return false;
    }

    // Time until next token is available (useful for "retry-after" headers)
    [[nodiscard]] double secondsUntilAllowed() const {
        if (tokens_ >= 1.0) return 0.0;
        return (1.0 - tokens_) / refill_rate_;
    }

    [[nodiscard]] double availableTokens() const { return tokens_; }

private:
    using Clock = std::chrono::steady_clock;

    double tokens_;
    double max_tokens_;
    double refill_rate_;  // tokens per second
    Clock::time_point last_refill_;

    void refill() {
        auto now = Clock::now();
        double elapsed = std::chrono::duration<double>(now - last_refill_).count();

        // Add tokens proportional to elapsed time, capped at max
        tokens_ = std::min(max_tokens_, tokens_ + elapsed * refill_rate_);
        last_refill_ = now;
    }
};

// Usage:
// RateLimiter limiter(10.0, 5.0);  // burst of 10, sustained 5 req/sec
//
// if (limiter.allow()) {
//     processRequest();
// } else {
//     double wait = limiter.secondsUntilAllowed();
//     respondWithRetryAfter(wait);  // HTTP 429 with Retry-After header
// }
```

**What makes this "strong hire" code:**
- `allowN(n)` for batch operations — shows you think beyond the basic API
- `secondsUntilAllowed()` — provides actionable information for callers (Retry-After header)
- Lazy refill — no background thread, no timer, fully deterministic
- `steady_clock` (not `system_clock`) — immune to wall-clock adjustments (NTP, DST)
- Clear constructor parameters with distinct names (`burst_capacity` vs `sustained_rate`)

### Production Extensions

- **Thread safety:** Wrap `allow()` with `std::mutex`. For lock-free, use `std::atomic<double>` with CAS loop on the token count.
- **Per-user limiting:** `std::unordered_map<UserId, RateLimiter>` with lazy construction on first request.
- **Distributed rate limiting:** Use Redis MULTI/EXEC or Lua scripts for atomic token operations across multiple application servers.
- **Hierarchical limiting:** Global limit (1M req/sec for the system) + per-user limit (100 req/sec per user). Request must pass both limiters.

---

## D13. Multi-Threaded Limit Order Book (L2/L3 Matching Engine)

### Problem Statement

Design a high-frequency Limit Order Book supporting:
- `addOrder(id, side, price, volume)` — Add a limit order with price-time priority
- `cancelOrder(id)` — Cancel an existing order in $O(1)$ time
- `matchOrders()` — Match matching bids and asks at top of book

### Why Bloomberg Asks This

Matching engines are the core of financial exchanges. Bloomberg Terminal users stream L2/L3 market data depth. This problem tests intrusively indexed data structures, price-time priority queues, and low-latency concurrency models.

### Solution Architecture

```
Bids (Descending Map: price → list<Order>):
  150.50 → [Order1(vol 100) ↔ Order2(vol 50)]
  150.25 → [Order3(vol 200)]

Asks (Ascending Map: price → list<Order>):
  150.75 → [Order4(vol 300)]

Intrusive Index (Hash Map):
  orderId → (Side, Price, list::iterator)
```

**Complexity:**
- `addOrder`: $O(\log P)$ where $P$ is distinct price levels
- `cancelOrder`: $O(1)$ via hash map lookup to `std::list::iterator`
- `matchOrders`: $O(1)$ per trade match execution

### Implementation

```cpp
#include <map>
#include <unordered_map>
#include <list>
#include <cstdint>
#include <shared_mutex>
#include <iostream>
#include <algorithm>

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

---

## D14. Distributed Market Data Ticker Stream Sequencer & Deduplicator

### Problem Statement

Reorder incoming out-of-order market data packet streams by sequence number and eliminate duplicate messages over a sliding time window.

### Solution Architecture

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

### Solution Architecture

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

### Solution Architecture

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

### Solution Architecture

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

