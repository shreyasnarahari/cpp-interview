# Templates & Metaprogramming — Easy Questions

---

**Q1. What is a function template?**

A: A blueprint for generating functions for different types. The compiler generates a concrete function for each type used (template instantiation).

```cpp
template <typename T>
T max(T a, T b) { return (a > b) ? a : b; }

max(3, 5);       // compiler generates max<int>
max(3.14, 2.71); // compiler generates max<double>
```

---

**Q2. What is template specialization?**

A: Providing a custom implementation for specific types:

```cpp
// Primary template
template <typename T>
std::string toString(T val) { return std::to_string(val); }

// Full specialization for std::string
template <>
std::string toString<std::string>(std::string val) { return val; }

// Partial specialization (only for class templates)
template <typename T>
class Container<T*> { /* pointer-specific impl */ };
```

---

**Q3. What is `constexpr`?**

A: Requests compile-time evaluation. If all inputs are known at compile time, the computation happens during compilation.

```cpp
constexpr int factorial(int n) {
    return (n <= 1) ? 1 : n * factorial(n - 1);
}

constexpr int f5 = factorial(5);  // computed at compile time = 120
int x = 5;
int fx = factorial(x);  // computed at runtime (x is not constexpr)
```

---

**Q4. What is `auto` type deduction?**

A: The compiler deduces the variable type from the initializer:

```cpp
auto x = 42;         // int
auto y = 3.14;       // double
auto s = "hello"s;   // std::string
auto v = std::vector{1,2,3};  // std::vector<int> (CTAD, C++17)
```

`auto` follows template argument deduction rules (with one exception: braced-init-list).

---

**Q5. What is CTAD (C++17)?**

A: Class Template Argument Deduction — lets the compiler deduce class template parameters from constructor arguments:

```cpp
std::vector v = {1, 2, 3};      // deduces std::vector<int>
std::pair p = {42, "hello"};    // deduces std::pair<int, const char*>
std::optional o = 3.14;         // deduces std::optional<double>
```

---

## Coding Questions

---

**C1. Generic `max` function**

Write a template function that returns the maximum of two values.

A:
```cpp
template <typename T>
const T& max(const T& a, const T& b) {
    return (a > b) ? a : b;
}
```

---

**C2. Generic `print_all` (variadic template)**

Print all arguments separated by spaces.

A:
```cpp
// Base case
void print_all() { std::cout << "\n"; }

// Recursive case
template <typename T, typename... Rest>
void print_all(const T& first, const Rest&... rest) {
    std::cout << first;
    if constexpr (sizeof...(rest) > 0) std::cout << " ";
    print_all(rest...);
}

print_all(1, "hello", 3.14, 'x');  // "1 hello 3.14 x"
```

Or with fold expressions (C++17):
```cpp
template <typename... Args>
void print_all(const Args&... args) {
    ((std::cout << args << " "), ...);
    std::cout << "\n";
}
```

---

**C3. `is_same` from scratch**

A:
```cpp
template <typename T, typename U>
struct is_same {
    static constexpr bool value = false;
};

template <typename T>
struct is_same<T, T> {  // partial specialization
    static constexpr bool value = true;
};

static_assert(is_same<int, int>::value);
static_assert(!is_same<int, double>::value);
```

---

**C4. Compile-time factorial**

A:
```cpp
// C++14 constexpr approach (preferred)
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) result *= i;
    return result;
}
static_assert(factorial(5) == 120);

// Classic template metaprogramming approach
template <int N>
struct Factorial {
    static constexpr int value = N * Factorial<N - 1>::value;
};
template <>
struct Factorial<0> {
    static constexpr int value = 1;
};
static_assert(Factorial<5>::value == 120);
```
