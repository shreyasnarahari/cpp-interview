# Theory & Mechanics: STL Containers & Iterators

## 1. Internal Data Structures & Memory Layouts

```
1. std::vector<T> Layout (Contiguous Memory):
   ┌───────────┬───────────┬───────────┬───────────┬───────────────────────┐
   │ Element 0 │ Element 1 │ Element 2 │ Element 3 │ Unallocated Storage   │
   └───────────┴───────────┴───────────┴───────────┴───────────────────────┘
   ^                       ^                       ^
   T* start                T* finish               T* end_of_storage

2. std::deque<T> Layout (Chunked Array Map):
   Block Map (Array of Pointers):
   ┌────────┬────────┬────────┬────────┐
   │ Block0 │ Block1 │ Block2 │ Block3 │
   └───│────┴───│────┴────────┴────────┘
       │        └─────────> ┌──────────┬──────────┬──────────┐ (Fixed-size Buffer)
       └──────────────────> ┌──────────┬──────────┬──────────┐

3. std::unordered_map<K, V> Layout (Separate Chaining):
   Bucket Array:
   ┌───┐    ┌─────────────────┐    ┌─────────────────┐
   │ 0 │───>│ Node(K1,V1) ptr ├───>│ Node(K4,V4) ptr │───> nullptr
   ├───┤    └─────────────────┘    └─────────────────┘
   │ 1 │───> nullptr
   └───┘
```

### Breakdown of Container Internals:

| Container | Internal Structure | Search | Insert (Front/Back) | Memory Locality |
|---|---|---|---|---|
| `std::vector` | Contiguous array | $O(N)$ / $O(\log N)$ sorted | Back $O(1)$ amortized, Front $O(N)$ | **Optimal** (L1 Cache line prefetch) |
| `std::deque` | Central map of fixed-size chunks | $O(N)$ | Front $O(1)$, Back $O(1)$ | Good (per chunk buffer) |
| `std::list` | Doubly-linked list nodes | $O(N)$ | Front $O(1)$, Back $O(1)$ | **Poor** (pointer chasing per node) |
| `std::map` | Red-Black Tree (Self-balancing BST) | $O(\log N)$ | $O(\log N)$ | Moderate (node pointer allocations) |
| `std::unordered_map` | Bucket array + linked list chains | $O(1)$ avg, $O(N)$ worst | $O(1)$ avg, $O(N)$ worst | Moderate |

---

## 2. `std::vector` Growth Factor & Amortized $O(1)$ Proof

### Growth Factor ($\gamma$)
When `size() == capacity()`, `push_back` allocates a new memory block of size $\gamma \times \text{capacity}$:
- **GCC `libstdc++` & LLVM `libc++`**: $\gamma = 2.0$ (Doubles capacity: 1, 2, 4, 8, 16, 32...).
- **MSVC STL**: $\gamma = 1.5$ (Golden ratio approximation: reduces memory fragmentation by allowing freed older blocks to be reused).

### Amortized $O(1)$ Complexity Proof
Consider inserting $N$ elements into an initially empty vector with growth factor $\gamma = 2$:
- Allocations occur at sizes $1, 2, 4, 8, \dots, 2^k$.
- Total copy operations for $N$ elements: $1 + 2 + 4 + 8 + \dots + N = 2N - 1$.
- Total cost across $N$ insertions = $N \text{ (regular inserts)} + 2N \text{ (reallocations)} = 3N$.
- Average cost per insertion = $\frac{3N}{N} = O(1)$ **amortized constant time**.

---

## 3. Comprehensive Iterator & Reference Invalidation Matrix

This table is one of the most heavily tested areas in C++ technical rounds:

| Container | Operation | Iterators Invalidated | References / Pointers Invalidated |
|---|---|---|---|
| **`std::vector`** | `push_back` / `insert` (with realloc) | **ALL** | **ALL** |
| | `push_back` / `insert` (no realloc) | Insertion point to `end()` | Insertion point to `end()` |
| | `erase` | Erased element to `end()` | Erased element to `end()` |
| **`std::deque`** | `push_front` / `push_back` | **ALL** | **NONE** (Element references remain valid!) |
| | `insert` / `erase` (in middle) | **ALL** | **ALL** |
| **`std::list`** | `insert` / `push_front` / `push_back` | **NONE** | **NONE** |
| | `erase` | Only erased elements | Only erased elements |
| **`std::map` / `set`** | `insert` / `emplace` | **NONE** | **NONE** |
| | `erase` | Only erased elements | Only erased elements |
| **`std::unordered_map`** | `insert` (if `rehash` occurs) | **ALL** | **NONE** (Nodes remain in place!) |
| | `insert` (no rehash) | **NONE** | **NONE** |
| | `erase` | Only erased elements | Only erased elements |

---

## 4. Operational Mechanics & Best Practices

### 1. `push_back` vs `emplace_back`
- **`push_back(const T&)` / `push_back(T&&)`**: Requires a constructed object (or temporary), which is then passed to copy or move constructor into vector storage.
- **`emplace_back(Args&&...)`**: Forwards arguments directly into placement `new` at the destination memory slot inside the vector, **constructing the object in-place** and bypassing temporary object creation!

### 2. `reserve()` vs `resize()`
- **`reserve(n)`**: Allocates memory capacity for at least `n` elements. Does NOT construct any objects. `size()` remains unchanged; `capacity()` becomes $\ge n$.
- **`resize(n)`**: Changes `size()` to `n`. Default-constructs new elements if $n > \text{size()}$, or destroys trailing elements if $n < \text{size()}$.

### 3. Remove-Erase Idiom
`std::remove_if` does not delete elements from a vector (it cannot alter the container's `size()`). Instead, it shifts non-matching elements forward and returns a logical end iterator:

```cpp
// Erase all odd numbers from vector
std::vector<int> v = {1, 2, 3, 4, 5, 6};

// std::remove_if shifts elements -> {2, 4, 6, ?, ?, ?} and returns iterator to 4th pos
v.erase(std::remove_if(v.begin(), v.end(), [](int x) { return x % 2 != 0; }), 
        v.end());
```

### 4. `std::span<T>` (C++20 Zero-Copy View)
`std::span<T>` is a non-owning lightweight view over a contiguous sequence of objects (like a pointer + size). It accepts `std::vector`, `std::array`, or C-style arrays without triggering allocations or copies.

---

## 5. Code Traps & Gotcha Scenarios

### Code Trap 1: Erasing Inside a Loop (`vector` Iterator Invalidation)
```cpp
// BUG: Erasing from vector invalidates current iterator!
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it % 2 == 0) v.erase(it); // INVALIDATES 'it'! ++it is UB!
}

// CORRECT SOLUTION: Use the return value of erase()
for (auto it = v.begin(); it != v.end(); /* no ++it */) {
    if (*it % 2 == 0) {
        it = v.erase(it); // erase returns iterator to next valid element
    } else {
        ++it;
    }
}
```

### Code Trap 2: `std::map::operator[]` Implicit Insertion
```cpp
std::map<int, int> m;
std::cout << m[5]; // Inserts key 5 with default-constructed value 0!
std::cout << " size=" << m.size(); // PRINTS: 0 size=1
// FIX: Use m.find(5) or m.contains(5) (C++20) for read-only lookups without insertion side-effects.
```

### Code Trap 3: Vector Reference Invalidation
```cpp
std::vector<int> v = {1, 2, 3};
int& ref = v[0];
v.push_back(4); // Reallocation occurs -> memory relocated!
std::cout << ref; // UB: 'ref' is a dangling reference!
```

---

## 6. Implementation Pattern: Production-Grade LRU Cache

```cpp
#include <unordered_map>
#include <list>
#include <utility>
#include <stdexcept>

template <typename Key, typename Value>
class LRUCache {
    size_t capacity_;
    using Pair = std::pair<Key, Value>;
    std::list<Pair> items_; // Most recently used at front
    using ListIt = typename std::list<Pair>::iterator;
    std::unordered_map<Key, ListIt> map_;

public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    Value get(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) throw std::out_of_range("Key not found");
        // Move accessed item to front of list (O(1))
        items_.splice(items_.begin(), items_, it->second);
        return it->second->second;
    }

    void put(const Key& key, Value value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            items_.splice(items_.begin(), items_, it->second); // Move to front
            return;
        }
        if (map_.size() >= capacity_) {
            // Evict Least Recently Used (back of list)
            auto lru_key = items_.back().first;
            map_.erase(lru_key);
            items_.pop_back();
        }
        items_.push_front({key, value});
        map_[key] = items_.begin();
    }
};
```
