# Undefined Behavior & Common Pitfalls — Easy Questions

Try to answer each question before reading the answer.

---

**Q1. What is undefined behavior?**

A: Code whose behavior the C++ standard places **no requirements on**. The compiler may do anything: crash, produce wrong results, appear to work, or cause seemingly unrelated code to break.

Common sources:
1. **Signed integer overflow**: `INT_MAX + 1`
2. **Null pointer dereference**: `*nullptr`
3. **Out-of-bounds access**: `arr[10]` on a size-5 array
4. **Use-after-free**: using memory after `delete`
5. **Dangling reference**: returning reference to local variable
6. **Data races**: unsynchronized concurrent access
7. **Double free**: `delete` on the same pointer twice

Why UB exists: it allows the compiler to make **optimizing assumptions** (e.g., signed overflow never happens → loop bounds can be simplified).

---

**Q2. What is the difference between UB, unspecified, and implementation-defined?**

A:

| Category | Definition | Example |
|---|---|---|
| **Undefined** | No requirements. Anything can happen. | Signed overflow, null deref |
| **Unspecified** | Valid options, compiler chooses one. Not documented. | Order of function argument evaluation (pre-C++17) |
| **Implementation-defined** | Valid options, compiler documents its choice. | `sizeof(int)` (must be documented: 4 on most platforms) |

```cpp
// Undefined — don't write this
int x = INT_MAX + 1;  // anything can happen

// Unspecified — don't depend on this
int a = 0;
f(a++, a++);  // which increment happens first? unspecified

// Implementation-defined — safe to use if you check docs
std::cout << sizeof(long);  // 4 or 8 depending on platform
```

---

**Q3. Is signed integer overflow defined?**

A: **No — it is undefined behavior.**

```cpp
int x = INT_MAX;
x = x + 1;  // UB! The compiler may assume this never happens.
```

**Unsigned** overflow IS defined — it wraps around:
```cpp
unsigned int u = UINT_MAX;
u = u + 1;  // defined: u == 0 (wraps around)
```

This is why the compiler can optimize `for (int i = 0; i < n; i++)` without worrying about `i` overflowing past `n`.

---

**Q4. What is a dangling reference?**

A: A reference (or pointer) to memory that has been freed or gone out of scope:

```cpp
// Dangling reference — returning reference to local
std::string& bad() {
    std::string s = "hello";
    return s;  // s destroyed at end of function — reference is dangling
}
std::string& ref = bad();
std::cout << ref;  // UB — accessing freed memory

// Dangling pointer — use after delete
int* p = new int(42);
delete p;
std::cout << *p;  // UB — p is dangling
```

Fix: return by value, or use smart pointers for heap objects.

---

**Q5. What are sanitizers? Name three.**

A: Runtime tools that detect bugs by instrumenting your code during compilation:

| Sanitizer | Detects | Flag |
|---|---|---|
| **AddressSanitizer (ASan)** | Buffer overflow, use-after-free, double-free, memory leaks | `-fsanitize=address` |
| **UndefinedBehaviorSanitizer (UBSan)** | Signed overflow, null deref, alignment violations, type errors | `-fsanitize=undefined` |
| **ThreadSanitizer (TSan)** | Data races, lock order inversions | `-fsanitize=thread` |
| **MemorySanitizer (MSan)** | Reads of uninitialized memory | `-fsanitize=memory` |
| **LeakSanitizer (LSan)** | Memory leaks (often part of ASan) | `-fsanitize=leak` |

```bash
# Compile with sanitizers:
g++ -fsanitize=address,undefined -g -o myapp myapp.cpp
./myapp  # crashes immediately at the first UB/bug with a clear error message
```

---

## Coding Questions

---

**C1. Spot the UB**

Find the undefined behavior:

```cpp
int arr[3] = {1, 2, 3};
for (int i = 0; i <= 3; i++) {
    std::cout << arr[i] << " ";
}
```

A: `arr[3]` is out-of-bounds — the array has indices 0, 1, 2. Accessing index 3 is UB. Fix: `i < 3`.

---

**C2. Fix the dangling pointer**

```cpp
int* createArray() {
    int arr[5] = {1, 2, 3, 4, 5};
    return arr;  // returns pointer to local array — dangling!
}
```

A:
```cpp
// Fix 1: return a vector (best)
std::vector<int> createArray() {
    return {1, 2, 3, 4, 5};
}

// Fix 2: heap allocation with smart pointer
std::unique_ptr<int[]> createArray() {
    auto arr = std::make_unique<int[]>(5);
    for (int i = 0; i < 5; i++) arr[i] = i + 1;
    return arr;
}
```

---

**C3. Safe integer addition**

Write a function that detects signed overflow before it happens:

A:
```cpp
#include <limits>

bool safeAdd(int a, int b, int& result) {
    if (b > 0 && a > std::numeric_limits<int>::max() - b) return false;  // overflow
    if (b < 0 && a < std::numeric_limits<int>::min() - b) return false;  // underflow
    result = a + b;
    return true;
}

// Usage:
int result;
if (!safeAdd(INT_MAX, 1, result)) {
    std::cerr << "Overflow detected!\n";
}
```

Or use compiler builtins:
```cpp
int result;
if (__builtin_add_overflow(a, b, &result)) {
    // overflow occurred
}
```

---

**C4. Unsigned comparison trap**

```cpp
// BUG: comparing signed and unsigned
std::vector<int> v = {1, 2, 3};
for (int i = 0; i < v.size(); i++) {  // warning: signed/unsigned comparison
    std::cout << v[i];
}
// Worse:
int x = -1;
if (x < v.size()) {  // FALSE! -1 is converted to a huge unsigned number
    std::cout << "less";
}
```

A: Fix: use `size_t` or `auto` for loop variable, or `std::ssize(v)` (C++20) for signed size:
```cpp
for (size_t i = 0; i < v.size(); i++) { ... }
// or
for (auto i = 0u; i < v.size(); i++) { ... }
// or (C++20)
for (auto i = 0; i < std::ssize(v); i++) { ... }
```
