# Modern C++ Features (C++11 through C++23) — Deep Theory & Mechanics

An exhaustive reference on modern C++ language evolution, feature mechanics, vocabulary types (`std::optional`, `std::variant`, `std::any`, `std::expected`), C++20 Coroutines, Modules, Concepts, Ranges, and the Three-Way Comparison Spaceship Operator (`<=>`).

---

## 1. Vocabulary Types: Safe Value Semantics

### 1.1 `std::optional<T>`
Represents a value that may or may not be present without heap allocation:

```cpp
std::optional<User> find_user(uint64_t id) {
    if (cache_.contains(id)) return cache_[id];
    return std::nullopt; // No user found
}

// Monadic Operations (C++23):
auto user_name = find_user(42)
    .and_then([](const User& u) { return u.get_profile(); })
    .transform([](const Profile& p) { return p.display_name; })
    .value_or("Anonymous");
```

---

### 1.2 `std::variant<Ts...>`: Type-Safe Discriminated Union
Stores exactly one of several alternative types in-place:

```cpp
std::variant<int, double, std::string> v = "Hello World";
std::cout << "Type index: " << v.index() << "\n"; // Index 2

if (std::holds_alternative<std::string>(v)) {
    std::cout << std::get<std::string>(v) << "\n";
}
```

---

### 1.3 `std::expected<T, E>` (C++23)
The modern standard for functional error handling without exceptions:

```cpp
std::expected<double, std::string> safe_divide(double a, double b) {
    if (b == 0.0) return std::unexpected("Division by zero error");
    return a / b;
}

auto res = safe_divide(10.0, 0.0);
if (res) {
    std::cout << "Result: " << *res << "\n";
} else {
    std::cerr << "Error: " << res.error() << "\n";
}
```

---

## 2. C++20 Three-Way Comparison (`<=>` Spaceship Operator)

The `<=>` operator automatically synthesizes all 6 relational operators (`<`, `<=`, `>`, `>=`, `==`, `!=`):

```cpp
struct Point3D {
    int x, y, z;

    // Compiler automatically generates all 6 comparisons in member order (x, then y, then z)!
    auto operator<=>(const Point3D&) const = default;
};
```

### Comparison Categories:
1. **`std::strong_ordering`**: Total ordering (e.g. integers, strings). If `a == b`, then `f(a) == f(b)`.
2. **`std::weak_ordering`**: Equivalent values may be distinct (e.g. case-insensitive string comparison: `"hello" == "HELLO"`).
3. **`std::partial_ordering`**: Some values are incomparable (e.g. IEEE 754 floating-point `NaN`: `NaN < 0` is false, `NaN > 0` is false, `NaN == 0` is false).

---

## 3. C++20 Coroutines Architecture

A C++20 coroutine is a function that can **suspend execution** and **resume later** without blocking the calling thread.

```
+-------------------------------------------------------------------------------+
| COROUTINE FUNCTION (`co_await`, `co_yield`, `co_return`)                      |
+---------------------------------------|---------------------------------------+
                                        v (Allocates Coroutine Frame on Heap)
+-------------------------------------------------------------------------------+
| COROUTINE STATE FRAME                                                         |
|  1. Promise Object (`promise_type`): Stores return value / yielded values     |
|  2. Parameters & Local Variables (Preserved across suspensions)               |
|  3. Suspension Point (Resume PC register offset)                              |
|  4. `std::coroutine_handle<>`: Execution control interface                    |
+-------------------------------------------------------------------------------+
```

### Custom Generator Coroutine Implementation:
```cpp
template <typename T>
struct Generator {
    struct promise_type {
        T current_value;
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(T value) noexcept {
            current_value = value;
            return {};
        }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle_;
    ~Generator() { if (handle_) handle_.destroy(); }

    bool next() {
        if (!handle_ || handle_.done()) return false;
        handle_.resume();
        return !handle_.done();
    }
    T value() const { return handle_.promise().current_value; }
};

Generator<int> count_up(int max) {
    for (int i = 1; i <= max; ++i) {
        co_yield i; // Suspends execution and returns i to caller!
    }
}
```

---

## 4. C++20 Modules (`import` vs `#include`)

```cpp
// math_utils.cppm (Module Interface Unit)
export module math_utils;

export int add(int a, int b) {
    return a + b;
}

// main.cpp (Consumer)
import math_utils;
int main() {
    return add(2, 3);
}
```

### Advantages Over Headers:
1. **Compilation Speed**: Module interfaces are compiled **once** into binary interface files (`.bmi`/`.pcm`), eliminating repetitive header parsing.
2. **Zero Macro Leaks**: Macros defined inside modules do not leak into importers!
3. **Order Independence**: The order of `import` statements does not affect compilation.
