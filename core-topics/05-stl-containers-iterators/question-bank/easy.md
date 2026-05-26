# STL Containers & Iterators — Easy Questions

---

**Q1. What is `std::vector`?**

A: A dynamic array with contiguous memory. Supports O(1) random access, amortized O(1) `push_back`, O(n) insert/erase in the middle.

```cpp
std::vector<int> v = {1, 2, 3};
v.push_back(4);       // amortized O(1)
v[0] = 10;            // O(1) random access
v.insert(v.begin(), 0); // O(n) — shifts all elements
```

Prefer `vector` as the default container — cache-friendly, fast, and versatile.

---

**Q2. What is the difference between `std::map` and `std::unordered_map`?**

A:
| | `std::map` | `std::unordered_map` |
|---|---|---|
| Internal structure | Red-black tree | Hash table |
| Lookup complexity | O(log n) | O(1) average, O(n) worst |
| Insertion | O(log n) | O(1) average |
| Ordering | Sorted by key | No ordering |
| Key requirement | `operator<` | `std::hash` + `operator==` |
| When faster | Small n, bad hash, need ordering | Large n, good hash |

---

**Q3. What is `push_back` vs `emplace_back`?**

A: `push_back` copies/moves an existing object. `emplace_back` constructs in-place (forwarding arguments to constructor).

```cpp
std::vector<std::string> v;
v.push_back("hello");         // creates temporary string, then moves it in
v.emplace_back("hello");      // constructs string directly in vector's memory
v.emplace_back(5, 'x');       // constructs std::string(5, 'x') = "xxxxx"
// v.push_back(5, 'x');       // ERROR — push_back takes one argument
```

`emplace_back` avoids one move/copy and supports constructor arguments directly.

---

**Q4. What is the remove-erase idiom?**

A: `std::remove` doesn't actually remove elements — it moves non-removed elements to the front and returns an iterator to the new "end". You must call `erase` to actually shrink the container.

```cpp
std::vector<int> v = {1, 2, 3, 2, 4, 2, 5};
// Remove all 2s:
v.erase(std::remove(v.begin(), v.end(), 2), v.end());
// v = {1, 3, 4, 5}

// C++20 simpler:
std::erase(v, 2);
```

---

**Q5. What is `reserve()` vs `resize()`?**

A:
- `reserve(n)` — allocates memory for n elements but doesn't create them. `size()` unchanged, `capacity()` ≥ n.
- `resize(n)` — changes the number of elements. Adds default-constructed elements or removes excess.

```cpp
std::vector<int> v;
v.reserve(100);  // capacity=100, size=0, no elements created
v.resize(100);   // capacity≥100, size=100, 100 zeros created
```

---

## Coding Questions

---

**C1. Erase-remove all odd numbers**

A:
```cpp
std::vector<int> v = {1, 2, 3, 4, 5, 6, 7};
v.erase(std::remove_if(v.begin(), v.end(),
    [](int x) { return x % 2 != 0; }), v.end());
// v = {2, 4, 6}

// C++20:
std::erase_if(v, [](int x) { return x % 2 != 0; });
```

---

**C2. Count frequency of each word**

A:
```cpp
std::unordered_map<std::string, int> freq;
std::vector<std::string> words = {"apple", "banana", "apple", "cherry", "banana", "apple"};
for (const auto& w : words) {
    freq[w]++;
}
// freq = {"apple": 3, "banana": 2, "cherry": 1}
```

---

**C3. Remove duplicates from sorted vector**

A:
```cpp
void removeDuplicates(std::vector<int>& v) {
    auto last = std::unique(v.begin(), v.end());
    v.erase(last, v.end());
}
// Or: v.erase(std::unique(v.begin(), v.end()), v.end());
```

---

**C4. Flatten nested vectors**

A:
```cpp
template <typename T>
std::vector<T> flatten(const std::vector<std::vector<T>>& nested) {
    std::vector<T> result;
    for (const auto& inner : nested) {
        result.insert(result.end(), inner.begin(), inner.end());
    }
    return result;
}
```
