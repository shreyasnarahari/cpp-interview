# STL Algorithms & Ranges — Real Interview Questions

---

**Q1. Implement an iterator adapter that filters elements. [Google, Meta]**

*Interviewer:* "I want you to write a custom iterator that wraps an existing `vector<int>::iterator`. However, when I increment your iterator, it should skip any odd numbers and only point to even numbers. This is essentially what `std::views::filter` does."

A:
```cpp
#include <iterator>

class EvenIterator {
    std::vector<int>::iterator current_;
    std::vector<int>::iterator end_;

    void advanceToEven() {
        while (current_ != end_ && *current_ % 2 != 0) {
            ++current_;
        }
    }

public:
    // Required typedefs for std::iterator_traits compatibility
    using iterator_category = std::forward_iterator_tag;
    using value_type        = int;
    using difference_type   = std::ptrdiff_t;
    using pointer           = int*;
    using reference         = int&;

    EvenIterator(std::vector<int>::iterator start, std::vector<int>::iterator end) 
        : current_(start), end_(end) {
        advanceToEven(); // ensure we start on a valid even element
    }

    // Dereference
    reference operator*() { return *current_; }
    pointer operator->() { return &(*current_); }

    // Pre-increment
    EvenIterator& operator++() {
        if (current_ != end_) {
            ++current_;
            advanceToEven();
        }
        return *this;
    }

    // Post-increment
    EvenIterator operator++(int) {
        EvenIterator temp = *this;
        ++(*this);
        return temp;
    }

    // Equality
    bool operator==(const EvenIterator& other) const { return current_ == other.current_; }
    bool operator!=(const EvenIterator& other) const { return current_ != other.current_; }
};
```
*Follow-up:* Why define `iterator_category` and the other types? 
Because standard algorithms (like `std::copy` or `std::distance`) look for `std::iterator_traits<T>` to optimize their execution. Without these typedefs, passing `EvenIterator` to STL algorithms would fail to compile.

---

**Q2. Two Sum in a sorted array [Amazon, Bloomberg]**

*Interviewer:* "Given a sorted array, find two numbers that add up to a target sum. Do it in $\mathcal{O}(1)$ space."

A: This is the classic two-pointer approach. While it can be written with integer indices, an interviewer evaluating modern C++ skills will want to see you write it using iterators.

```cpp
bool hasTwoSum(const std::vector<int>& v, int target) {
    if (v.empty()) return false;
    
    auto left = v.begin();
    auto right = std::prev(v.end()); // Safer than v.end() - 1

    while (left < right) {
        int sum = *left + *right;
        if (sum == target) return true;
        
        if (sum < target) {
            ++left;
        } else {
            --right;
        }
    }
    return false;
}
```
*Alternative approach:* Iterate through the array and use `std::binary_search(it + 1, v.end(), target - *it)` to find the complement. This is $\mathcal{O}(N \log N)$, whereas two-pointers is $\mathcal{O}(N)$.

---

**Q3. Meeting Rooms II (Find max overlapping intervals) [Meta, Google]**

*Interviewer:* "Given an array of meeting time intervals consisting of start and end times `[[s1,e1],[s2,e2],...]`, find the minimum number of conference rooms required."

A: A clean C++ solution separates the start times and end times, sorts them, and uses two pointers. This heavily relies on `std::sort`.

```cpp
int minMeetingRooms(const std::vector<std::pair<int, int>>& intervals) {
    std::vector<int> starts, ends;
    starts.reserve(intervals.size());
    ends.reserve(intervals.size());

    for (const auto& interval : intervals) {
        starts.push_back(interval.first);
        ends.push_back(interval.second);
    }

    std::sort(starts.begin(), starts.end());
    std::sort(ends.begin(), ends.end());

    int rooms = 0, maxRooms = 0;
    auto s_it = starts.begin();
    auto e_it = ends.begin();

    while (s_it != starts.end()) {
        // If a meeting starts before the earliest ending meeting finishes, we need a new room
        if (*s_it < *e_it) {
            rooms++;
            maxRooms = std::max(maxRooms, rooms);
            s_it++;
        } else {
            // A meeting ended, free up a room
            rooms--;
            e_it++;
        }
    }
    return maxRooms;
}
```

---

**Q4. Find the Duplicate Number (Immutable Array) [Microsoft, Amazon]**

*Interviewer:* "Given an array of integers `nums` containing $N + 1$ integers where each integer is in the range `[1, N]` inclusive. There is only one repeated number. Return the repeated number. You must not modify the array and must use $\mathcal{O}(1)$ extra space."

A: Since we cannot modify the array (no sorting), and cannot use $\mathcal{O}(N)$ space (no hash maps), we can use Binary Search on the *value range* `[1, N]` leveraging `std::count_if`.

```cpp
int findDuplicate(const std::vector<int>& nums) {
    int low = 1;
    int high = nums.size() - 1;
    int duplicate = -1;
    
    while (low <= high) {
        int mid = low + (high - low) / 2;
        
        // Count how many numbers are less than or equal to 'mid'
        int count = std::count_if(nums.begin(), nums.end(), [mid](int num) {
            return num <= mid;
        });
        
        if (count > mid) {
            // By Pigeonhole Principle, the duplicate is in [low, mid]
            duplicate = mid;
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    
    return duplicate;
}
```
*Follow-up:* Time complexity is $\mathcal{O}(N \log N)$. Can you do it in $\mathcal{O}(N)$ time? Yes, using Floyd's Tortoise and Hare (Cycle Detection) algorithm, treating the array values as pointers to the next index.
