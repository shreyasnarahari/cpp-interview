# Theory & Mechanics: STL Algorithms & Ranges

## 1. Algorithm Complexity Guarantees

The standard mandates exact complexity requirements — interviewers expect you to know these:

| Algorithm | Complexity | Notes |
|---|---|---|
| `std::sort` | O(n log n) average and worst | Introsort: quicksort → heapsort fallback |
| `std::stable_sort` | O(n log n) if extra memory available, O(n log² n) otherwise | Merge sort |
| `std::partial_sort(f, m, l)` | O(n log k), k = m - f | Heap-based — top-K elements |
| `std::nth_element` | O(n) average | Introselect: quickselect → median-of-medians fallback |
| `std::lower_bound` | O(log n) | Requires sorted range |
| `std::find` | O(n) | Linear scan — unsorted |
| `std::binary_search` | O(log n) | Returns bool only — prefer `lower_bound` |

## 2. `std::sort` Internals — Introsort

Most STL implementations use **Introsort** (introspective sort):

```
Introsort algorithm:
1. Start with Quicksort (fast in practice due to cache locality)
2. Track recursion depth
3. If depth exceeds 2 × log₂(n):
   → Switch to Heapsort (O(n log n) guaranteed worst-case)
4. When partition size < ~16 elements:
   → Switch to Insertion Sort (O(n²) but fast for tiny arrays due to low overhead)

This gives:
  - Average case: O(n log n) — quicksort's speed
  - Worst case:   O(n log n) — heapsort's guarantee
  - Small arrays:  fast — insertion sort's low constant factor
```

## 3. `std::nth_element` — O(n) Selection

Rearranges so that the element at position `nth` is what would be there if sorted. Elements before `nth` are ≤ it, elements after are ≥ it, but neither side is sorted:

```cpp
std::vector<int> v = {9, 3, 7, 1, 5, 8, 2, 4, 6};
std::nth_element(v.begin(), v.begin() + 4, v.end());
// v[4] == 5 (the median)
// v[0..3] are all ≤ 5 (but unsorted)
// v[5..8] are all ≥ 5 (but unsorted)

// Finding top-K: combine with partial_sort for sorted top-K:
std::partial_sort(v.begin(), v.begin() + 3, v.end(), std::greater{});
// v[0..2] = top 3 elements, sorted descending
```

## 4. Erase-Remove Idiom

`std::remove` does NOT remove elements — it **moves unwanted elements to the end** and returns an iterator to the new logical end:

```cpp
std::vector<int> v = {1, 2, 3, 2, 4, 2, 5};

auto new_end = std::remove(v.begin(), v.end(), 2);
// v is now: {1, 3, 4, 5, ?, ?, ?}
//                        ↑ new_end
// The elements after new_end are in a valid-but-unspecified state

v.erase(new_end, v.end());  // actually shrink the container
// v is now: {1, 3, 4, 5}

// C++20 simplification — single call:
std::erase(v, 2);           // erase all 2s
std::erase_if(v, is_even);  // erase all matching predicate
```

## 5. C++20 Ranges — Lazy Evaluation & Composition

Ranges transform algorithm pipelines from eager (compute everything) to **lazy** (compute on demand):

```cpp
#include <ranges>

std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// Traditional (eager — allocates intermediate vectors):
std::vector<int> temp;
std::copy_if(data.begin(), data.end(), std::back_inserter(temp),
             [](int x) { return x % 2 == 0; });
std::vector<int> result;
std::transform(temp.begin(), temp.end(), std::back_inserter(result),
               [](int x) { return x * x; });

// Ranges (lazy — ZERO intermediate allocations):
auto result = data
    | std::views::filter([](int x) { return x % 2 == 0; })
    | std::views::transform([](int x) { return x * x; })
    | std::views::take(3);

// result is a VIEW — no computation has happened yet!
// Computation happens lazily when iterated:
for (int x : result) {
    std::cout << x << " ";  // 4 16 36
}
```

### How views work internally:
```
Each view stores:
  - A reference to the underlying range (or the previous view)
  - The predicate/transformation function

When you iterate:
  1. take_view requests next element from transform_view
  2. transform_view requests next element from filter_view
  3. filter_view scans underlying data, skipping non-matching elements
  4. Found element flows back up: filter → transform → take → caller

No intermediate containers are created — it's a pull-based pipeline.
```

## 6. Projections (C++20)

Projections let you specify which member or transformation to use without writing a custom comparator:

```cpp
struct Student { std::string name; int grade; };
std::vector<Student> students = { {"Alice", 95}, {"Bob", 87}, {"Carol", 92} };

// Without projections — verbose:
std::ranges::sort(students, [](const Student& a, const Student& b) {
    return a.grade < b.grade;
});

// With projections — clean:
std::ranges::sort(students, std::less{}, &Student::grade);
// Reads: sort students by less-than, projecting on grade
```
