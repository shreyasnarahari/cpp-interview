# STL Containers & Iterators — Deep Theory & Mechanics

An exhaustive, low-level guide to C++ Standard Template Library (STL) container internal physical layouts, growth algorithms, node architectures, hash table bucket mechanics, iterator invalidation rules, cache locality analysis, and custom iterator implementations.

---

## 1. Sequence Containers: Physical Architecture & Memory Models

### 1.1 `std::vector<T>`: Contiguous Array Buffer

```
std::vector<T> (24 Bytes on 64-bit platforms):
+--------------------+--------------------+--------------------+
| T* start_          | T* finish_ (Size)  | T* end_of_storage_ |
+---------|----------+---------|----------+---------|----------+
          |                    |                    |
          v                    v                    v
Heap:    [Elem 0][Elem 1]...[Elem N-1]             [Unused Capacity]
         <------- size() ------------>
         <----------------- capacity() -------------------->
```

- **Growth Factor**:
  - GCC / Clang (`libstdc++`, `libc++`): **$2.0\times$** multiplier.
  - MSVC: **$1.5\times$** multiplier (theoretically allows reusing previously freed heap memory blocks!).
- **Amortized $O(1)$ Complexity Proof**:
  - Inserting $N$ elements requires doubling at sizes $1, 2, 4, 8, \dots, N$.
  - Total copied elements: $1 + 2 + 4 + \dots + N = 2N - 1$.
  - Amortized cost per insertion: $\frac{2N - 1}{N} \approx 2 = O(1)$!
- **`reserve(N)` vs `resize(N)`**:
  - `reserve(N)`: Allocates raw capacity for at least $N$ elements without calling constructors. `size()` is unchanged.
  - `resize(N)`: Changes `size()` to $N$, default-constructing new elements if $N > \text{size()}$, or destroying trailing elements if $N < \text{size()}$.

---

### 1.2 `std::deque<T>`: Map of Fixed-Size Chunks

`std::deque` (Double-Ended Queue) avoids the massive reallocation copy cost of `std::vector` by allocating fixed-size contiguous memory blocks (typically 512 bytes each) indexed by a central pointer map.

```
std::deque Central Map (Array of Pointers):
+--------+--------+--------+--------+--------+
| Block0 | Block1 | Block2 | Block3 | Block4 |
+---|----+---|----+---|----+--------+--------+
    |        |        |
    v        v        v
  [512B]   [512B]   [512B] (Fixed Contiguous Chunks)
```

- **Invariants**:
  - $O(1)$ insertions/deletions at **both front and back**.
  - **Zero element reallocation**: Adding elements never copies or moves existing elements (only the central map pointer array may reallocate!).
  - **Random access**: $O(1)$ indexing via arithmetic: `chunk_idx = i / CHUNK_SIZE`, `elem_idx = i % CHUNK_SIZE`.

---

### 1.3 `std::list<T>` & `std::forward_list<T>`: Node-Based Doubly/Singly Linked Lists

```
std::list Node Layout:
+-------------------+-------------------+-------------------+
| Node* prev (8B)   | Node* next (8B)   | T data (e.g. 4B)  | + 4B Padding
+-------------------+-------------------+-------------------+
<----------------------- 24 Bytes Total -------------------->
```

- **Overhead**: 16 bytes of pointer metadata per element + heap chunk allocator header (8–16 bytes). For an `int` (4B), memory overhead is **$>600\%$**!
- **Cache Locality Penalty**: Each node is allocated independently on the heap, causing CPU L1/L2 cache misses on every traversal step ("pointer chasing"). In benchmarks, `std::vector` is typically **$10\times - 50\times$ faster** than `std::list` even for random insertions!

---

## 2. Associative & Hash Containers

### 2.1 `std::map<K, V>` & `std::set<K>`: Red-Black Self-Balancing Trees

```
Red-Black Tree Node (32-40 Bytes per element):
+----------------+----------------+----------------+----------------+---------+
| Node* parent   | Node* left     | Node* right    | Color (1 Byte) | K key   |
+----------------+----------------+----------------+----------------+---------+
```

- **Invariants**: Search, Insertion, Deletion all guaranteed **$O(\log N)$**.
- **Iterator Stability**: Inserting or deleting a node never invalidates iterators to other nodes.
- **Transparent Comparators (`std::less<>`)**:
  - Allows searching with heterogeneous types without allocating temporaries (e.g., searching `std::map<std::string, int>` using `std::string_view` or `const char*`).

---

### 2.2 `std::unordered_map<K, V>`: Bucket Array with Separate Chaining

```
Bucket Array (Array of Node Pointers):
[Bucket 0] -> [Node (Hash=0, Key="AAPL", Val=150)] -> [Node (Hash=0, Key="GOOG", Val=2800)]
[Bucket 1] -> nullptr
[Bucket 2] -> [Node (Hash=2, Key="MSFT", Val=300)]
```

- **Load Factor**: $\text{load\_factor} = \frac{\text{size()}}{\text{bucket\_count()}}$. Default `max_load_factor` is $1.0$.
- **Rehashing**: When `load_factor > max_load_factor`, the bucket array doubles in size, and all elements are re-distributed. Complexity: $O(N)$.
- **Worst-Case Trap**: Hash collision attacks or poor hash functions degrade lookup complexity from average $O(1)$ to worst-case $O(N)$!

---

## 3. The Master Iterator Invalidation Matrix

| Container | Operation | Iterator Invalidation Rule | Pointer / Reference Invalidation |
|---|---|---|---|
| **`std::vector`** | `push_back` / `insert` (Capacity Reallocation) | **ALL iterators invalidated!** | **ALL pointers/references invalidated!** |
| | `push_back` / `insert` (No Reallocation) | Iterators at or after insertion point invalidated. | Pointers/references remain VALID. |
| | `erase` / `pop_back` | Iterators at or after erasure point invalidated. | Pointers/references at or after erasure invalidated. |
| **`std::deque`** | Insertion at Front or Back | **ALL iterators invalidated!** | **Pointers/references remain VALID!** |
| | Insertion in Middle | ALL iterators invalidated. | ALL pointers/references invalidated. |
| | Erase at Front or Back | Only erased iterator invalidated. | Only erased pointer/ref invalidated. |
| **`std::list`** | `insert` / `push_front` / `push_back` | **No iterators invalidated!** | **No pointers/references invalidated!** |
| | `erase` | Only erased iterator invalidated. | Only erased pointer/ref invalidated. |
| **`std::map` / `set`** | `insert` / `emplace` | **No iterators invalidated!** | **No pointers/references invalidated!** |
| | `erase` | Only erased iterator invalidated. | Only erased pointer/ref invalidated. |
| **`std::unordered_map`**| `insert` (Triggers Rehash) | **ALL iterators invalidated!** | **Pointers/references remain VALID!** |
| | `insert` (No Rehash) | No iterators invalidated. | No pointers/references invalidated. |
| | `erase` | Only erased iterator invalidated. | Only erased pointer/ref invalidated. |

---

## 4. Modern Erase-Remove Idiom (C++20 Uniform `std::erase`)

```cpp
// C++98 / C++11 / C++17 Idiom:
auto it = std::remove_if(vec.begin(), vec.end(), [](int x) { return x % 2 == 0; });
vec.erase(it, vec.end());

// Modern C++20 Uniform Erase:
std::erase_if(vec, [](int x) { return x % 2 == 0; });
```

---

## 5. Container Adapters & Heap Algorithms

`std::priority_queue` is backed by `std::vector` and operates via binary heap algorithms:
- `std::make_heap(begin, end)`: Transforms array into max-heap in linear **$O(N)$** time.
- `std::push_heap(begin, end)`: Sifts up the newly appended element in **$O(\log N)$**.
- `std::pop_heap(begin, end)`: Swaps top element with the last element and sifts down in **$O(\log N)$**.
