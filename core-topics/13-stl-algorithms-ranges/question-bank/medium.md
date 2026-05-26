# STL Algorithms & Ranges — Medium Questions

---

**Q1. What is the difference between `std::lower_bound` and `std::upper_bound`?**

A: Both perform a binary search on a sorted range and run in $\mathcal{O}(\log N)$ time.
- `std::lower_bound(begin, end, val)` returns an iterator to the **first element that is NOT less than** `val` (i.e., $\ge$ `val`).
- `std::upper_bound(begin, end, val)` returns an iterator to the **first element that is STRICTLY greater than** `val` (i.e., $>$ `val`).

If the element does not exist, both return an iterator to the first element greater than `val` (which is where you would insert `val` to maintain sorted order).

Together, they define the range of elements equivalent to `val`, which can also be found directly using `std::equal_range`.

---

**Q2. When would you use `std::partition` vs `std::sort`?**

A: `std::sort` puts all elements in a total order ($\mathcal{O}(N \log N)$). 

`std::partition` merely divides the container into two groups based on a predicate: elements returning `true` come first, and those returning `false` come second. It runs in $\mathcal{O}(N)$ time.

If you only need to group elements (e.g., all even numbers before all odd numbers, or all valid items before invalid items), `std::partition` is strictly superior. Use `std::stable_partition` if you need to preserve the relative order of elements within the two groups (runs in $\mathcal{O}(N \log N)$ if not enough extra memory is available).

---

**Q3. What does `std::rotate` do? Give a practical use case.**

A: `std::rotate(first, middle, last)` performs a left rotation such that the element at `middle` becomes the new `first` element.

**Practical Use Case**: It is heavily used for sliding elements around without allocating new memory. For example, moving an element from the back of a vector to the front:
```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
// Move the last element (5) to the front
std::rotate(v.rbegin(), v.rbegin() + 1, v.rend());
// v is now {5, 1, 2, 3, 4}
```
Or moving a specific element to a new position (Insertion Sort mechanics).

---

**Q4. Explain C++20 Ranges vs. traditional STL algorithms.**

A: Traditional STL algorithms take a begin and end iterator pair:
`std::sort(v.begin(), v.end());`

C++20 Ranges accept the container directly:
`std::ranges::sort(v);`

Furthermore, `<ranges>` introduces **Views** (`std::views`). Views are lazy-evaluated, non-owning adapters that can be composed using the pipe `|` operator.
```cpp
auto evens_squared = v 
    | std::views::filter([](int i){ return i % 2 == 0; })
    | std::views::transform([](int i){ return i * i; });
```
**Crucial Difference**: Traditional `<algorithm>` functions (like `std::transform`) eagerly process all elements and write them to memory immediately. **Views** do absolutely nothing until you iterate over them. They evaluate on-demand (lazily).

---

## Gotcha Code

```cpp
// Q: Is this code safe?
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it == 3) {
        v.insert(it, 99);
    }
}

// A: NO! Undefined Behavior. 
// std::vector::insert may cause reallocation, which invalidates ALL iterators.
// Even if it doesn't reallocate, it shifts elements, making 'it' invalid.
// Fix: it = v.insert(it, 99); ++it; // Re-assign the iterator and skip the new element
```

---

## Coding Questions

**C1. Merge Overlapping Intervals**

Given a vector of intervals, merge all overlapping ones.

A:
```cpp
struct Interval {
    int start, end;
};

std::vector<Interval> mergeIntervals(std::vector<Interval>& intervals) {
    if (intervals.empty()) return {};

    // 1. Sort by start time using C++20 ranges and a projection
    std::ranges::sort(intervals, std::less{}, &Interval::start);

    std::vector<Interval> merged;
    merged.push_back(intervals[0]);

    for (size_t i = 1; i < intervals.size(); i++) {
        if (merged.back().end >= intervals[i].start) {
            // Overlapping, update end
            merged.back().end = std::max(merged.back().end, intervals[i].end);
        } else {
            // No overlap
            merged.push_back(intervals[i]);
        }
    }
    return merged;
}
```

---

**C2. Move Zeroes (In-Place)**

Given an integer array, move all `0`s to the end of it while maintaining the relative order of the non-zero elements.

A: This is exactly what `std::stable_partition` does!

```cpp
void moveZeroes(std::vector<int>& nums) {
    std::stable_partition(nums.begin(), nums.end(), [](int n) {
        return n != 0; 
    });
}
```
*Follow-up:* If relative order doesn't matter, what would you use? `std::partition` is faster because it just swaps from both ends.

---

**C3. Generating Permutations**

Write a function to generate all unique permutations of a string.

A:
```cpp
void printPermutations(std::string s) {
    // String MUST be sorted for next_permutation to generate ALL permutations
    std::sort(s.begin(), s.end()); 
    
    do {
        std::cout << s << "\n";
    } while (std::next_permutation(s.begin(), s.end()));
}
```
`std::next_permutation` modifies the sequence in-place to the lexicographically next permutation. It returns `false` when it cycles back to the fully sorted order.
