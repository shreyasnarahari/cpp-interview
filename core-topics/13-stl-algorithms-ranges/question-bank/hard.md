# STL Algorithms & Ranges — Hard Questions

---

**Q1. When would you use `std::nth_element` vs `std::partial_sort`?**

A: Both are used for finding top-K elements or median finding, but their output guarantees and complexities differ:

- **`std::nth_element(begin, nth, end)`**: Reorders the elements such that the element at the `nth` iterator is the exact element that *would* be in that position if the array was sorted. It guarantees that all elements before `nth` are $\le$ the `nth` element, and all elements after are $\ge$. It does **not** sort the elements before or after `nth`. 
  - **Time complexity**: $\mathcal{O}(N)$ average case. (It uses an algorithm similar to Quickselect).
- **`std::partial_sort(begin, middle, end)`**: Reorders the elements such that the range `[begin, middle)` contains the smallest elements in the entire range, **and they are fully sorted**. 
  - **Time complexity**: $\mathcal{O}(N \log K)$ where $K$ is the distance from begin to middle. (It typically builds a max-heap of size K).

**Rule of thumb**: If you just need the top 10 elements (but don't care about their relative order), use `nth_element`. If you need the top 10 elements *in sorted order*, use `partial_sort`.

---

**Q2. What is a Projection in C++20 Ranges?**

A: A projection is an invokable (like a lambda or member pointer) passed to an algorithm to transform elements *before* the algorithm operates on them. This removes the need for writing complex, error-prone custom comparators.

Instead of writing a comparator:
```cpp
std::sort(employees.begin(), employees.end(), 
    [](const Employee& a, const Employee& b) { return a.salary < b.salary; });
```
With C++20 ranges and projections, you just write:
```cpp
std::ranges::sort(employees, std::less{}, &Employee::salary);
```
Here, `&Employee::salary` is the projection. The algorithm projects each employee to their salary under the hood, and then compares the salaries.

---

**Q3. How does `std::priority_queue` use heap algorithms?**

A: `std::priority_queue` is a container adapter that wraps an underlying container (usually `std::vector`). It relies on three crucial `<algorithm>` functions:
- `std::make_heap(begin, end)`: Turns the vector into a max-heap in $\mathcal{O}(N)$ time.
- `std::push_heap(begin, end)`: Called *after* you `push_back` an element to the vector. It bubbles the new element up to restore the heap property in $\mathcal{O}(\log N)$ time.
- `std::pop_heap(begin, end)`: Swaps the root (largest) element with the last element and bubbles the new root down to restore the heap property in $\mathcal{O}(\log N)$ time. The user then calls `pop_back` on the vector to actually remove the element.

---

**Q4. Explain Execution Policies (C++17).**

A: You can pass an execution policy to most STL algorithms to enable parallel execution.
- `std::execution::seq`: Sequential execution (default).
- `std::execution::par`: Parallel execution. The algorithm can run on multiple threads.
- `std::execution::par_unseq`: Parallel and unsequenced. Elements can be processed using SIMD instructions (vectorization) alongside threading.

*Gotcha*: When using `par`, the user is responsible for avoiding data races. If the lambda you pass to `std::for_each` modifies shared state without a mutex, it will result in undefined behavior.

---

## Gotcha Code

```cpp
// Q: What's wrong with this lambda?
int removals = 0;
v.erase(std::remove_if(v.begin(), v.end(), [&removals](int x) {
    if (x % 2 == 0) {
        removals++;
        return true;
    }
    return false;
}), v.end());

// A: std::remove_if requires the predicate to be pure (stateless).
// The standard does NOT guarantee how many times the predicate is called per element, 
// nor does it guarantee the order of invocation.
// In practice, it might copy the lambda, and the captured reference might yield 
// an unpredictable count. Never use stateful lambdas in STL algorithms!
```

---

## Coding Questions

**C1. K Closest Points to Origin**

Given an array of points where `points[i] = [xi, yi]` and an integer `K`, return the `K` closest points to the origin `(0, 0)`.

A:
The best C++ solution uses `std::nth_element`. Since we only need the K closest points (and they don't need to be sorted relative to each other), $\mathcal{O}(N)$ Quickselect is optimal.

```cpp
struct Point {
    int x, y;
    int distSq() const { return x*x + y*y; }
};

std::vector<Point> kClosest(std::vector<Point>& points, int K) {
    std::nth_element(points.begin(), points.begin() + K - 1, points.end(),
        [](const Point& a, const Point& b) {
            return a.distSq() < b.distSq();
        }
    );
    // Return a copy of the first K elements
    return std::vector<Point>(points.begin(), points.begin() + K);
}
```

---

**C2. Custom Views/Ranges Adapter Pipeline**

Using C++20 Ranges, write a function that takes a vector of strings, filters out strings shorter than 3 characters, converts them to uppercase, and prints the first 5 results. Do not allocate any intermediate vectors.

A:
```cpp
#include <iostream>
#include <vector>
#include <string>
#include <ranges>

void printTop5LongUppercase(const std::vector<std::string>& words) {
    auto to_upper = [](std::string s) { // pass by value to modify
        for (char& c : s) c = std::toupper(c);
        return s;
    };

    auto pipeline = words 
        | std::views::filter([](const std::string& s) { return s.length() >= 3; })
        | std::views::transform(to_upper)
        | std::views::take(5);

    for (const auto& word : pipeline) {
        std::cout << word << "\n";
    }
}
```
*Note the elegance:* No intermediate vectors are allocated. `filter` and `transform` are evaluated lazily only when the `for` loop requests the next element. Evaluation stops completely after 5 elements are printed due to `take(5)`.
