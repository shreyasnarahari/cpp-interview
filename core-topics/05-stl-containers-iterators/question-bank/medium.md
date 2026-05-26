# STL Containers — Medium Questions

---

**Q1. Explain iterator invalidation for `std::vector`.**

A:
| Operation | Iterators invalidated |
|---|---|
| `push_back` (no realloc) | None (end() invalidated) |
| `push_back` (realloc) | **ALL** iterators, pointers, references |
| `insert` (no realloc) | From insertion point to end |
| `insert` (realloc) | **ALL** |
| `erase` | From erasure point to end |
| `clear` | **ALL** |
| `reserve` | **ALL** (may reallocate) |

```cpp
std::vector<int> v = {1, 2, 3};
auto it = v.begin();
v.push_back(4);    // if capacity exceeded → it is INVALID
// *it is UB!
```

---

**Q2. When is `std::map` faster than `std::unordered_map`?**

A:
1. **Small datasets** (n < ~100): tree traversal has less overhead than hashing
2. **Poor hash function**: worst case O(n) for hash collisions
3. **Need sorted traversal**: map is already sorted, unordered_map would need sorting
4. **Stable iterators**: map iterators are never invalidated by insert; unordered_map iterators may be invalidated by rehash

---

**Q3. How does vector's growth work internally?**

A: When `size == capacity` and you `push_back`:
1. Allocate new buffer of size `capacity * growth_factor` (typically 1.5x or 2x)
2. Move (or copy) all elements to new buffer
3. Destroy old elements
4. Deallocate old buffer
5. Update internal pointer, size, capacity

GCC/libstdc++ uses 2x. MSVC uses 1.5x. Amortized O(1) per push_back because total copies across n pushes = O(n).

---

**Q4. What is `emplace` vs `insert` for maps?**

A:
```cpp
std::map<std::string, int> m;

// insert — constructs pair, then copies/moves into map
m.insert({"hello", 42});
m.insert(std::make_pair("hello", 42));

// emplace — constructs pair in-place
m.emplace("hello", 42);  // no temporary pair

// try_emplace (C++17) — doesn't move value if key exists
std::string key = "hello";
m.try_emplace(key, 42);  // doesn't construct value if key already exists
```

---

## Coding Questions

---

**C1. LRU Cache**

A:
```cpp
class LRUCache {
    int capacity_;
    std::list<std::pair<int, int>> cache_;  // front = most recent
    std::unordered_map<int, std::list<std::pair<int,int>>::iterator> map_;

public:
    LRUCache(int cap) : capacity_(cap) {}

    int get(int key) {
        auto it = map_.find(key);
        if (it == map_.end()) return -1;
        // Move to front (most recently used)
        cache_.splice(cache_.begin(), cache_, it->second);
        return it->second->second;
    }

    void put(int key, int value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            cache_.splice(cache_.begin(), cache_, it->second);
            return;
        }
        if ((int)cache_.size() == capacity_) {
            // Evict least recently used (back)
            map_.erase(cache_.back().first);
            cache_.pop_back();
        }
        cache_.emplace_front(key, value);
        map_[key] = cache_.begin();
    }
};
```

O(1) get and put. `std::list` for O(1) move-to-front, `unordered_map` for O(1) lookup.

---

**C2. Safe iteration with erasure**

A:
```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto it = v.begin(); it != v.end(); ) {
    if (*it % 2 == 0) {
        it = v.erase(it);  // erase returns next valid iterator
    } else {
        ++it;
    }
}
```

---

**C3. Merge two sorted vectors**

A:
```cpp
std::vector<int> merge(const std::vector<int>& a, const std::vector<int>& b) {
    std::vector<int> result;
    result.reserve(a.size() + b.size());
    std::merge(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(result));
    return result;
}
```

---

**C4. Top-K elements using `priority_queue`**

A:
```cpp
std::vector<int> topK(const std::vector<int>& v, int k) {
    // Min-heap of size k
    std::priority_queue<int, std::vector<int>, std::greater<int>> pq;
    for (int x : v) {
        pq.push(x);
        if ((int)pq.size() > k) pq.pop();
    }
    std::vector<int> result;
    while (!pq.empty()) {
        result.push_back(pq.top());
        pq.pop();
    }
    return result;  // top-k in ascending order
}
```
