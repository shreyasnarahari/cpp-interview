# Theory & Mechanics: STL Algorithms & Ranges

## 1. Introsort: `std::sort` Internal Hybrid Architecture

`std::sort` does NOT use plain Quicksort. It uses **Introsort** (Introspective Sort), a hybrid algorithm combining three sorting techniques to achieve optimal performance:

```
                          Introsort Execution Pipeline:
                                  std::sort()
                                       │
                         [ Is Subarray Size < 16? ]
                                   /       \
                             YES  /         \  NO
                                 /           \
                       Insertion Sort    Quicksort Partitioning
                       (O(N) for small)       │
                                        [ Recursion Depth > 2*log2(N)? ]
                                             /          \
                                       YES  /            \  NO
                                           /              \
                                     Heapsort           Continue Quicksort
                                     (Guarantees O(N log N) worst-case)
```

### 1. Quicksort Phase
Executes Partitioning with median-of-three pivot selection for fast average $O(N \log N)$ execution.

### 2. Heapsort Fallback (Guaranteed $O(N \log N)$ Worst-Case)
If recursion depth exceeds $2 \lfloor \log_2 N \rfloor$ (indicating a bad pivot sequence that would degenerate into $O(N^2)$), Introsort switches to **Heapsort** for that subtree.

### 3. Insertion Sort Final Pass
When a partition size drops below a small threshold (typically 16 elements), Introsort stops recursing. A single final pass of **Insertion Sort** is run over the near-sorted array ($O(N)$ execution due to high CPU L1 cache hit rate).

---

## 2. Strict Weak Ordering Requirements (CRITICAL BUG TRAP)

Custom comparator functions passed to `std::sort`, `std::map`, or `std::set` MUST satisfy **Strict Weak Ordering**:

1. **Irreflexivity**: `comp(x, x)` MUST be **`false`**.
2. **Asymmetry**: If `comp(x, y)` is `true`, `comp(y, x)` MUST be `false`.
3. **Transitivity**: If `comp(x, y)` and `comp(y, z)` are `true`, then `comp(x, z)` MUST be `true`.

### The Bad Comparator Heap Corruption Bug:
```cpp
// BUG: Using <= instead of < violates Irreflexivity!
std::sort(v.begin(), v.end(), [](int a, int b) {
    return a <= b; // comp(5, 5) returns TRUE -> Violates Strict Weak Ordering!
});
// RESULT: Buffer Overflow / Heap Corruption UB inside std::sort partitioning!
```

---

## 3. Algorithm Complexity & Guarantees Table

| Algorithm | Average Time | Worst Case Time | Space Complexity | Stability |
|---|---|---|---|---|
| `std::sort` | $O(N \log N)$ | $O(N \log N)$ | $O(\log N)$ | Unstable |
| `std::stable_sort` | $O(N \log N)$ | $O(N \log^2 N)$ (if no memory) | $O(N)$ | **Stable** |
| `std::partial_sort` | $O(N \log K)$ | $O(N \log K)$ | $O(1)$ | Unstable |
| `std::nth_element` | $O(N)$ | $O(N)$ | $O(1)$ | Unstable |
| `std::lower_bound` | $O(\log N)$ | $O(\log N)$ | $O(1)$ | — |

---

## 4. Parallel Execution Policies (C++17)

C++17 enables parallel execution of standard algorithms via `<execution>` policies:

```cpp
#include <algorithm>
#include <execution>

std::vector<int> data(10'000'000);

// Parallel + SIMD Vectorization Execution Policy:
std::sort(std::execution::par_unseq, data.begin(), data.end());
```

- `std::execution::seq`: Sequential execution (same thread).
- `std::execution::par`: Parallel execution across multiple worker threads.
- `std::execution::par_unseq`: Parallel execution + SIMD vectorization (operations may be interleaved on the same thread across SIMD registers).

---

## 5. Custom Iterator Adapters & `iterator_traits`

Custom iterators must define 5 required nested types (or specialize `std::iterator_traits<T>`) so STL algorithms can inspect iterator capabilities:

```cpp
#include <iterator>

template <typename T>
class SimpleVectorIterator {
    T* ptr_;
public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    explicit SimpleVectorIterator(T* ptr) : ptr_(ptr) {}
    reference operator*() const { return *ptr_; }
    SimpleVectorIterator& operator++() { ++ptr_; return *this; }
    bool operator==(const SimpleVectorIterator& other) const { return ptr_ == other.ptr_; }
};
```
