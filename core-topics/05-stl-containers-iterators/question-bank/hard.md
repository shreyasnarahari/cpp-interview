# STL Containers — Hard Questions

---

**Q1. Explain `std::deque`'s internal structure.**

A: A deque is an **array of fixed-size blocks** (typically 512 bytes each). A central map (array of block pointers) tracks the blocks.

```
Central map:    [ptr0] [ptr1] [ptr2] [ptr3]
                  ↓      ↓      ↓      ↓
Blocks:       [....] [1234] [5678] [....]
                      ↑               ↑
                    begin             end
```

- O(1) push_front/push_back (add to existing block or allocate new block)
- O(1) random access (compute block index + offset)
- NOT contiguous — `&d[0] + n != &d[n]` in general
- Iterators invalidated by insert at front/back (but references/pointers are NOT)

---

**Q2. What is `std::span` (C++20)?**

A: A non-owning view over contiguous data. Like `string_view` but for any array-like data:

```cpp
void process(std::span<const int> data) {
    for (int x : data) std::cout << x << " ";
}

std::vector<int> v = {1, 2, 3};
int arr[] = {4, 5, 6};
std::array<int, 3> a = {7, 8, 9};

process(v);    // works
process(arr);  // works
process(a);    // works
process({v.data() + 1, 2});  // subrange: {2, 3}
```

Zero overhead — just a pointer + size. Fixed-extent `span<int, 5>` or dynamic `span<int>`.

---

**Q3. What is the Small String Optimization (SSO)?**

A: Short strings (typically ≤ 15-22 chars depending on implementation) are stored directly inside the `std::string` object, not on the heap.

```cpp
// Without SSO: string = { ptr, size, capacity } → ptr → heap buffer
// With SSO:    string = { char[22], size } → no heap allocation!

std::string s = "hello";  // likely no heap allocation (SSO)
std::string s = "this is a much longer string that won't fit in SSO";  // heap
```

SSO eliminates heap allocation for the vast majority of strings. This is why `std::string` move is not always faster than copy for short strings.

---

**Q4. What is a hash table's load factor and when does rehashing occur?**

A: Load factor = `size / bucket_count`. When it exceeds `max_load_factor` (default 1.0), the table rehashes:
1. Allocate new, larger bucket array
2. Re-hash all elements into new buckets
3. All iterators invalidated

```cpp
std::unordered_map<int, int> m;
m.max_load_factor(0.75);  // rehash when 75% full
m.reserve(1000);           // pre-allocate for 1000 elements
std::cout << m.bucket_count();  // at least ceil(1000/0.75) = 1334
```

---

## Coding Questions

---

**C1. Implement a simple hash map**

A:
```cpp
template <typename K, typename V>
class HashMap {
    struct Entry { K key; V value; bool occupied = false; };
    std::vector<Entry> table_;
    size_t size_ = 0;

    size_t hash(const K& key) const {
        return std::hash<K>{}(key) % table_.size();
    }

    size_t probe(const K& key) const {
        size_t idx = hash(key);
        while (table_[idx].occupied && table_[idx].key != key) {
            idx = (idx + 1) % table_.size();  // linear probing
        }
        return idx;
    }

public:
    HashMap(size_t capacity = 16) : table_(capacity) {}

    void put(const K& key, const V& value) {
        if (size_ * 2 >= table_.size()) rehash();
        size_t idx = probe(key);
        if (!table_[idx].occupied) size_++;
        table_[idx] = {key, value, true};
    }

    std::optional<V> get(const K& key) const {
        size_t idx = probe(key);
        if (table_[idx].occupied) return table_[idx].value;
        return std::nullopt;
    }

private:
    void rehash() {
        auto old = std::move(table_);
        table_.assign(old.size() * 2, Entry{});
        size_ = 0;
        for (auto& e : old) {
            if (e.occupied) put(e.key, e.value);
        }
    }
};
```

---

**C2. Range-based algorithms (C++20)**

```cpp
// Filter even numbers, square them, take first 5
auto result = std::views::iota(1)
    | std::views::filter([](int x) { return x % 2 == 0; })
    | std::views::transform([](int x) { return x * x; })
    | std::views::take(5);

for (int x : result) std::cout << x << " ";
// Output: 4 16 36 64 100
```

Ranges are lazy — no intermediate containers are created.

---

**C3. Benchmark: vector vs list for insert-at-front**

```cpp
#include <chrono>
// Despite O(n) complexity, vector often beats list for insert-at-front
// up to moderate sizes due to cache locality.

void benchVector(int n) {
    std::vector<int> v;
    for (int i = 0; i < n; i++) v.insert(v.begin(), i);  // O(n) per insert
}

void benchList(int n) {
    std::list<int> l;
    for (int i = 0; i < n; i++) l.push_front(i);  // O(1) per insert
}
// For n < ~10000, vector is often faster due to contiguous memory and CPU caching
// For n > ~100000, list wins due to O(1) vs O(n) complexity
```

---

**C4. Custom hash for composite key**

A:
```cpp
struct Point { int x, y; };

struct PointHash {
    size_t operator()(const Point& p) const {
        auto h1 = std::hash<int>{}(p.x);
        auto h2 = std::hash<int>{}(p.y);
        return h1 ^ (h2 << 1);  // combine hashes
    }
};

struct PointEqual {
    bool operator()(const Point& a, const Point& b) const {
        return a.x == b.x && a.y == b.y;
    }
};

std::unordered_map<Point, std::string, PointHash, PointEqual> pointMap;
pointMap[{1, 2}] = "A";
```
