# Theory & Mechanics: STL Containers & Iterators

## 1. `std::vector` — Growth Strategy & Memory Layout

`vector` stores elements in a **contiguous heap-allocated array** with three pointers:

```
vector<int> v = {10, 20, 30};    capacity = 4 (implementation-defined)

vector object (stack):          Heap buffer:
┌──────────────────┐            ┌────┬────┬────┬────┐
│ begin_ ──────────────────────→│ 10 │ 20 │ 30 │ ?? │
│ end_   ──────────────────────────────────────→↑    │
│ cap_   ───────────────────────────────────────────→│
└──────────────────┘            └────┴────┴────┴────┘
sizeof(vector) = 3 × sizeof(void*) = 24 bytes (always, regardless of element count)
```

**Amortized O(1) `push_back`:** When `size == capacity`, vector allocates a new buffer of **2× capacity** (GCC/Clang) or **1.5×** (MSVC), moves all elements, and frees the old buffer. The amortization comes from the geometric growth — each element is moved O(1) times on average.

**Why 2× vs 1.5×?** With factor 2×, freed blocks can never be reused (sum of all previous sizes < current size). With 1.5×, after several reallocations, freed blocks can be coalesced and reused by the allocator.

## 2. `std::deque` — Block Array Architecture

`deque` is NOT a contiguous array. It's an array of pointers to **fixed-size blocks**:

```
deque<int>:
                 Block Map (array of pointers):
                 ┌─────┬─────┬─────┬─────┐
                 │  *  │  *  │  *  │  *  │
                 └──┬──┴──┬──┴──┬──┴──┬──┘
                    │     │     │     │
                    ↓     ↓     ↓     ↓
                 ┌─────┐┌─────┐┌─────┐┌─────┐
                 │Block││Block││Block││Block│  (each ~512 bytes)
                 └─────┘└─────┘└─────┘└─────┘
```

- **O(1) front/back insert:** Add new blocks at either end of the map
- **O(1) random access:** `block_map[index / BLOCK_SIZE][index % BLOCK_SIZE]`
- **NOT contiguous:** Cache performance is worse than `vector` for sequential scans

## 3. `std::map` / `std::set` — Red-Black Tree

```
map<int, string>:
                    ┌───────────┐
                    │ 20 (BLACK)│
                    └─────┬─────┘
                   ╱             ╲
          ┌───────────┐    ┌───────────┐
          │ 10 (RED)  │    │ 30 (RED)  │
          └───────────┘    └───────────┘

Each node: { key, value, color, left*, right*, parent* }
Node overhead: ~48-64 bytes per entry (vs 4 bytes for vector<int>)
```

- **O(log n)** for insert, find, erase — guaranteed (balanced tree)
- Iteration is **in-order** (sorted by key)
- Each node is a **separate heap allocation** → poor cache locality
- Iterators are **never invalidated** by insert (only erase invalidates the erased iterator)

## 4. `std::unordered_map` — Hash Table

```
unordered_map<string, int>:

Bucket array:            Chains (linked lists):
┌───────┐
│ [0] ──────→ nullptr
│ [1] ──────→ {"cat", 3} → {"dog", 7} → nullptr
│ [2] ──────→ nullptr
│ [3] ──────→ {"fish", 1} → nullptr
│ [4] ──────→ nullptr
│ ...   │
└───────┘
```

- **O(1) average** for insert/find/erase; **O(n) worst-case** with hash collisions
- **load_factor** = size / bucket_count. When `load_factor > max_load_factor` (default 1.0), the table **rehashes** (reallocates bucket array, re-inserts all elements — O(n))
- Rehashing **invalidates all iterators**

### When `std::map` beats `std::unordered_map`:
1. **Small n** (< ~100 elements): Tree traversal fits in cache; hash computation overhead dominates
2. **Bad hash function**: Collisions degrade to O(n) linked list traversal
3. **Need ordering**: Sorted iteration, `lower_bound`, range queries

## 5. Iterator Invalidation — Complete Rules

| Container | Insert | Erase |
|---|---|---|
| `vector` | All iterators/refs invalid if reallocation; otherwise only those at/after insertion point | At/after erased element |
| `deque` | All iterators invalid; refs valid IF insert at front/back | All iterators invalid; refs valid IF erase at front/back |
| `list` | None invalidated | Only the erased element |
| `map`/`set` | None invalidated | Only the erased element |
| `unordered_map` | All invalid IF rehash; otherwise none | Only the erased element |

## 6. `std::span` (C++20) — Non-Owning View

`span` is a **lightweight, non-owning** view over contiguous memory (like `string_view` for arrays):

```cpp
// span is just two values — a pointer and a size:
// sizeof(std::span<int>) == 16 bytes (pointer + size)

void process(std::span<const int> data) {
    for (int x : data) { /* ... */ }
}

std::vector<int> v = {1, 2, 3, 4, 5};
int arr[] = {10, 20, 30};

process(v);    // works — vector is contiguous
process(arr);  // works — array is contiguous
process({v.data() + 1, 3});  // works — subrange view
```

`span` replaces the error-prone `(T* ptr, size_t len)` function signature pattern.
