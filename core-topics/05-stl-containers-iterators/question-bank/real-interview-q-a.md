# STL Containers & Iterators — Real Interview Questions

> No answers — attempt cold first.

## Theory Questions
1. What are the different container categories in C++ STL?
2. When would you use `vector` vs `list` vs `deque`?
3. What is the difference between `std::map` and `std::unordered_map`?
4. Explain iterator invalidation rules for `std::vector`.
5. Explain iterator invalidation rules for `std::map` and `std::unordered_map`.
6. What is `push_back` vs `emplace_back`?
7. What is `reserve()` vs `resize()` for `std::vector`?
8. What is the remove-erase idiom? Why is it needed?
9. What are the iterator categories? Name all of them.
10. What is `std::span` (C++20)? When is it useful?
11. How does `std::vector` grow when capacity is exceeded?
12. What is the Small Buffer/String Optimization (SBO/SSO)?
13. When is `std::map` faster than `std::unordered_map`?
14. What is the complexity of `std::priority_queue` operations?

## Code Questions

```cpp
// Q1: What's wrong?
std::vector<int> v = {1,2,3,4,5};
for (auto it = v.begin(); it != v.end(); ++it)
    if (*it % 2 == 0) v.erase(it);
```

```cpp
// Q2: Is this safe?
std::vector<int> v = {1,2,3};
int& ref = v[0];
v.push_back(4);
std::cout << ref;
```

```cpp
// Q3: What does this output?
std::map<int, int> m;
std::cout << m[5];
std::cout << " size=" << m.size();
```

```cpp
// Q4: What's the complexity?
std::vector<int> v(1000000);
v.insert(v.begin(), 42);
```

## Implementation Problems
1. Implement an LRU cache using `std::list` and `std::unordered_map`
2. Write a function to remove duplicates from a sorted vector in O(n)
3. Implement a `flatten` function for `vector<vector<T>>`
4. Implement the erase-remove idiom for removing all odd numbers from a vector
