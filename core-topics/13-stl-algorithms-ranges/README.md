# Topic 13: STL Algorithms & Ranges (C++20)

## Why this topic matters

In FAANG interviews, especially for Senior positions, writing raw `for` and `while` loops is often considered an anti-pattern when an STL algorithm can express the intent more clearly. 

Interviewers look for engineers who say what they want to achieve (declarative) rather than exactly how they want to achieve it (imperative). 

With C++20, **Ranges (`<ranges>`)** evolved STL algorithms to be composable and lazy-evaluated, fundamentally changing how data pipelines are written in modern C++.

## Core Concepts to Master

1. **The Erase-Remove Idiom**: The classic C++ way to remove elements from a vector.
2. **Binary Search Family**: `std::lower_bound`, `std::upper_bound`, `std::equal_range`, `std::binary_search`.
3. **Partitioning**: `std::partition`, `std::stable_partition` (essential for Quicksort-like problems).
4. **Sorting and Selection**: `std::sort`, `std::partial_sort` (top K elements), `std::nth_element` (median finding).
5. **Set Operations**: `std::set_union`, `std::set_intersection` (often used in merge-interval type problems).
6. **C++20 Ranges & Views**: 
   - **Lazy evaluation**: Views do not compute elements until they are iterated.
   - **Composability**: `std::views::filter(pred) | std::views::transform(op)`
   - **Projections**: Pass a projection function to an algorithm instead of a complex custom comparator.

## 80/20 Rule for Interviews

If you only have time to master three things here:
1. **`std::lower_bound`**: Master how to use it with custom comparators. It is the solution to 90% of binary search interview questions.
2. **`std::nth_element`**: The O(N) solution to "find the Kth largest element" or "find the median".
3. **Erase-Remove Idiom**: Knowing `v.erase(std::remove(v.begin(), v.end(), val), v.end())` shows you actually write C++ in production.
