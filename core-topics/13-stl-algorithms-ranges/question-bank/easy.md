# STL Algorithms & Ranges — Easy Questions

---

**Q1. How do you remove an element from a `std::vector` by value?**

A: Historically, you use the **erase-remove idiom**:

```cpp
std::vector<int> v = {1, 2, 3, 2, 4, 2, 5};
v.erase(std::remove(v.begin(), v.end(), 2), v.end());
```

**Explanation**: 
- `std::remove` iterates through the vector. When it finds a value that is *not* 2, it copies it to the front of the array. It returns an iterator pointing to the new logical end of the array.
- Crucially, `std::remove` **does not change the size of the vector**. The elements past the new logical end are left in an unspecified (but valid) state.
- `std::vector::erase` takes the iterator returned by `std::remove` and actually shrinks the vector, destroying the leftover elements.

**Modern C++20 Update**: The standard library added a non-member `std::erase` to simplify this:
```cpp
std::erase(v, 2); // Does the exact same thing!
std::erase_if(v, [](int x){ return x % 2 == 0; });
```

---

**Q2. How do you find an element in a `std::vector`? How about a `std::set`?**

A: For `std::vector` (unsorted), use the generic `std::find`:
```cpp
auto it = std::find(v.begin(), v.end(), 42);
if (it != v.end()) { /* found */ }
```
Complexity: $\mathcal{O}(N)$.

For `std::set`, `std::map`, or their `unordered_` variants, **always** use the member function `.find()`:
```cpp
auto it = mySet.find(42);
```
Complexity: $\mathcal{O}(\log N)$ for ordered, $\mathcal{O}(1)$ for unordered. 
*Gotcha*: If you write `std::find(mySet.begin(), mySet.end(), 42)`, it will compile and work, but it ignores the container's internal tree/hash structure and performs a linear $\mathcal{O}(N)$ search, defeating the purpose of using a set!

---

**Q3. How do you count elements, or sum them up?**

A: 
- To count matching conditions, use `std::count_if`:
```cpp
int evens = std::count_if(v.begin(), v.end(), [](int x) { return x % 2 == 0; });
```
- To sum elements, use `std::accumulate` (requires `<numeric>`):
```cpp
int sum = std::accumulate(v.begin(), v.end(), 0); 
// Note: The '0' defines the return type. If summing doubles, use '0.0'!
```
- In C++17, use `std::reduce` for parallelized summing:
```cpp
int sum = std::reduce(std::execution::par, v.begin(), v.end(), 0);
```

---

**Q4. What is `std::iota`?**

A: Defined in `<numeric>`, it fills a range with successively increasing values. It is the C++ equivalent of Python's `range()`.
```cpp
std::vector<int> v(100);
std::iota(v.begin(), v.end(), 1); // Fills v with 1, 2, 3... 100
```

---

**Q5. How do you safely copy elements from one vector to another?**

A: Use `std::copy` or `std::copy_if` along with `std::back_inserter`.
```cpp
std::vector<int> src = {1, 2, 3, 4, 5};
std::vector<int> dest;

// WRONG: std::copy(src.begin(), src.end(), dest.begin()); // Undefined Behavior! dest is empty!

// CORRECT:
std::copy_if(src.begin(), src.end(), std::back_inserter(dest), [](int x) {
    return x > 2;
});
```
`std::back_inserter` creates an output iterator that calls `push_back()` on the container for every element written to it.

---

## Gotcha Code

```cpp
// Q: What is wrong with this code?
std::vector<int> v = {1, 2, 3, 4, 5};
std::remove(v.begin(), v.end(), 3);
std::cout << v.size();

// A: It prints 5! 
// std::remove only shuffles elements forward and returns a new end iterator. 
// It does NOT erase anything from the container. 
// The vector likely contains {1, 2, 4, 5, 5} now.
```

---

## Coding Questions

**C1. Reverse words in a string**

Given a string like `"hello world"`, reverse it to `"world hello"` in-place.

A:
```cpp
void reverseWords(std::string& s) {
    // 1. Reverse the entire string: "dlrow olleh"
    std::reverse(s.begin(), s.end());
    
    // 2. Reverse each individual word
    auto start = s.begin();
    while (start != s.end()) {
        auto end = std::find(start, s.end(), ' '); // Find next space
        std::reverse(start, end);                  // Reverse the word
        start = (end == s.end()) ? end : std::next(end);
    }
}
```

---

**C2. Check if a string is a palindrome**

Write a highly optimized one-liner to check if a string is a palindrome using algorithms.

A:
```cpp
bool isPalindrome(const std::string& s) {
    // std::equal compares two ranges. We only need to check the first half
    // against the reverse iterator of the string.
    return std::equal(s.begin(), s.begin() + s.size() / 2, s.rbegin());
}
```
