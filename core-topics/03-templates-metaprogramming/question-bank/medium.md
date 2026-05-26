# Templates & Metaprogramming — Medium Questions

---

**Q1. What is SFINAE?**

A: **Substitution Failure Is Not An Error.** When the compiler substitutes template arguments and the result is invalid, that overload is silently removed from the candidate set instead of causing a compile error.

```cpp
// Only enable for integral types
template <typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
double_it(T val) {
    return val * 2;
}

double_it(42);    // OK — int is integral
// double_it(3.14); // No matching function — substitution fails silently
```

---

**Q2. What are variadic templates?**

A: Templates that accept a variable number of type parameters (parameter pack):

```cpp
template <typename... Args>  // Args is a parameter pack
void log(const Args&... args) {
    (std::cout << ... << args) << "\n";  // fold expression
}

log("x=", 42, " y=", 3.14);  // "x=42 y=3.14"
```

`sizeof...(Args)` gives the number of parameters at compile time.

---

**Q3. What is `if constexpr` (C++17)?**

A: A compile-time `if` — the discarded branch is not instantiated:

```cpp
template <typename T>
auto process(T val) {
    if constexpr (std::is_integral_v<T>) {
        return val * 2;      // only compiled for integral types
    } else if constexpr (std::is_floating_point_v<T>) {
        return val + 0.5;    // only compiled for float types
    } else {
        return val;           // fallback
    }
}
```

Without `constexpr`, both branches would need to compile for all types → errors.

---

**Q4. What are fold expressions (C++17)?**

A: Compact syntax for reducing parameter packs:

```cpp
// Unary right fold: (pack op ...)
template <typename... Args>
auto sum(Args... args) { return (args + ...); }

// Unary left fold: (... op pack)
template <typename... Args>
auto sum_left(Args... args) { return (... + args); }

// Binary fold with init: (init op ... op pack)
template <typename... Args>
auto sum_init(Args... args) { return (0 + ... + args); }

sum(1, 2, 3, 4);  // 10
```

All four forms: unary left/right, binary left/right.

---

**Q5. What are type traits?**

A: Compile-time queries about types:

```cpp
std::is_integral_v<int>          // true
std::is_floating_point_v<double> // true
std::is_pointer_v<int*>          // true
std::is_same_v<int, int32_t>     // true (on most platforms)
std::is_base_of_v<Base, Derived> // true

// Transformations:
std::remove_reference_t<int&>     // int
std::remove_const_t<const int>    // int
std::decay_t<const int&>          // int
std::conditional_t<true, int, double>  // int
```

---

## Coding Questions

---

**C1. `enable_if` overloading**

Write two versions of `serialize`: one for integral types (print as number), one for strings (print with quotes).

A:
```cpp
template <typename T>
std::enable_if_t<std::is_integral_v<T>, std::string>
serialize(T val) {
    return std::to_string(val);
}

template <typename T>
std::enable_if_t<std::is_same_v<T, std::string>, std::string>
serialize(T val) {
    return "\"" + val + "\"";
}

// C++20 concepts version (cleaner):
template <std::integral T>
std::string serialize(T val) { return std::to_string(val); }

std::string serialize(const std::string& val) { return "\"" + val + "\""; }
```

---

**C2. Variadic `make_vector`**

Write a function that creates a vector from variadic arguments.

A:
```cpp
template <typename T, typename... Args>
std::vector<T> make_vector(Args&&... args) {
    std::vector<T> v;
    v.reserve(sizeof...(args));
    (v.push_back(std::forward<Args>(args)), ...);  // fold expression
    return v;
}

auto v = make_vector<int>(1, 2, 3, 4, 5);
```

---

**C3. Compile-time Fibonacci**

A:
```cpp
template <int N>
struct Fibonacci {
    static constexpr int value = Fibonacci<N-1>::value + Fibonacci<N-2>::value;
};
template <> struct Fibonacci<0> { static constexpr int value = 0; };
template <> struct Fibonacci<1> { static constexpr int value = 1; };

static_assert(Fibonacci<10>::value == 55);

// Modern constexpr version:
constexpr int fib(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}
static_assert(fib(10) == 55);
```

---

**C4. Tag dispatch**

Implement an `advance` function that moves an iterator forward, optimized for random access vs forward iterators.

A:
```cpp
namespace detail {
    template <typename It>
    void advance_impl(It& it, int n, std::random_access_iterator_tag) {
        it += n;  // O(1) for random access
    }

    template <typename It>
    void advance_impl(It& it, int n, std::input_iterator_tag) {
        for (int i = 0; i < n; i++) ++it;  // O(n) for input/forward
    }
}

template <typename It>
void advance(It& it, int n) {
    detail::advance_impl(it, n,
        typename std::iterator_traits<It>::iterator_category{});
}
```
