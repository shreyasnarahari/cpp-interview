# STL Algorithms & C++20 Ranges — Deep Theory & Mechanics

An exhaustive guide to Standard Template Library (STL) algorithms, algorithmic complexity bounds, Introsort internals, binary search mechanics (`lower_bound` vs `upper_bound`), partitioning, heap operations, and C++20 Ranges & Views.

---

## 1. Algorithm Complexity & Sorting Mechanics

### 1.1 `std::sort`: The Introsort Hybrid Algorithm
`std::sort` guarantees **$O(N \log N)$ worst-case time complexity** through a 3-stage hybrid architecture:

```
                          [ Input Array (N Elements) ]
                                       |
                                       v (Start Quicksort with Median-of-3 Pivot)
                          +-------------------------+
                          |   Quicksort Partition   |
                          +-------------------------+
                            /                     \
      (Recursion depth > 2 * log2(N))         (Sub-array size < 16-32 elements)
                          /                         \
                         v                           v
             +-----------------------+   +-----------------------+
             |   Heapsort Fallback   |   |    Insertion Sort     |
             | (Guarantees O(N log N)|   | (Optimal Cache/Branch)|
             +-----------------------+   +-----------------------+
```

### 1.2 Comparison Matrix of Sorting Algorithms

| Algorithm | Average Complexity | Worst-Case Complexity | Memory Overhead | Use Case |
|---|---|---|---|---|
| **`std::sort`** | $O(N \log N)$ | $O(N \log N)$ | $O(\log N)$ stack | General sorting when order of equals does not matter. |
| **`std::stable_sort`** | $O(N \log N)$ | $O(N \log^2 N)$ | $O(N)$ buffer | Preserves relative order of equal elements (Mergesort). |
| **`std::partial_sort`** | $O(N \log K)$ | $O(N \log K)$ | $O(1)$ | Finds and sorts the top $K$ smallest elements. |
| **`std::nth_element`** | **$O(N)$ (Linear)** | $O(N^2)$ (or $O(N)$ Introselect) | $O(1)$ | Finds the element that would be at index $K$ if sorted (Quickselect / Median). |

---

## 2. Binary Search: `lower_bound` vs `upper_bound`

Both algorithms operate in **$O(\log N)$** time on sorted ranges:

```
Sorted Range: [ 10, 20, 20, 20, 30, 40 ]
                       ^           ^
                       |           |
lower_bound(20) -------+           |  (Points to FIRST element >= 20, index 1)
upper_bound(20) -------------------+  (Points to FIRST element > 20,  index 4)

std::equal_range(20) returns pair [lower_bound, upper_bound) -> Range of all 20s!
```

---

## 3. Partitioning & Rotation

- **`std::partition`**: Reorganizes elements so all matching a predicate precede those that don't ($O(N)$ time, unstable).
- **`std::rotate(first, middle, last)`**: Performs left rotation, moving `middle` to `first` in $O(N)$ time. Useful for sliding window and circular shifts.

---

## 4. C++20 Ranges & Views Architecture

C++20 Ranges modernize STL algorithms through **Composable Pipelines (`|`)**, **Lazy Evaluation**, and **Projections**.

### 4.1 Lazy Pipelines & Zero-Allocation Views
Views do not own or allocate data; they operate lazily on-demand:

```cpp
std::vector<int> numbers = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// Lazy Range Pipeline:
auto result = numbers 
    | std::views::filter([](int n) { return n % 2 == 0; }) // Lazy filter
    | std::views::transform([](int n) { return n * n; })   // Lazy square
    | std::views::take(3);                                 // Take first 3

for (int v : result) {
    std::cout << v << " "; // Prints: 4 16 36 (Computed on the fly!)
}
```

### 4.2 Projections: Eliminating Custom Lambda Boilerplate
Projections extract a sub-field or transformation directly within algorithms:

```cpp
struct Employee {
    std::string name;
    int salary;
};

std::vector<Employee> staff = { {"Alice", 120000}, {"Bob", 95000}, {"Charlie", 150000} };

// Sort directly by member variable 'salary' using projection (no custom lambda needed!):
std::ranges::sort(staff, {}, &Employee::salary);
```

### 4.3 `std::ranges::dangling` Iterator Protection
Prevents dangling iterators when calling algorithms on temporary rvalue containers:

```cpp
// ERROR at compile-time! Returning iterator into a temporary vector is caught!
// auto it = std::ranges::max_element(std::vector<int>{1, 5, 3}); // Returns std::ranges::dangling!
```
