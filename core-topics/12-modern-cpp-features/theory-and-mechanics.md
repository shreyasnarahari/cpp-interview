# Theory & Mechanics: Modern C++ Features

## 1. Structured Bindings (C++17) — Decomposition

Structured bindings provide syntactic sugar to decompose aggregates, tuples, and pairs:

```cpp
std::map<std::string, int> scores = {{"Alice", 95}, {"Bob", 87}};

for (const auto& [name, score] : scores) {
    std::cout << name << ": " << score << "\n";
}

// Compiler generates approximately:
for (const auto& __e : scores) {
    const auto& name = __e.first;
    const auto& score = __e.second;
    // ...
}
```

Works with: `std::pair`, `std::tuple`, `std::array`, structs with all-public members, and any type that specializes `std::tuple_size` and `std::get`.

## 2. `std::optional` / `std::variant` — No Heap Allocation

Both store their value **inline** using aligned storage — no dynamic allocation:

```
std::optional<int>:
┌──────────────┬──────────┐
│ bool engaged │ int data │   sizeof = 8 (4 + padding + 4)
└──────────────┴──────────┘
  If engaged == false, data is uninitialized (no value)

std::variant<int, double, std::string>:
┌─────────────┬────────────────────────────────┐
│ size_t index│ aligned_union<int,double,string>│  sizeof = max(types) + index + padding
└─────────────┴────────────────────────────────┘
  index tracks which alternative is currently active
  Accessing wrong alternative throws std::bad_variant_access
```

### `std::visit` — Pattern matching for variants:
```cpp
std::variant<int, double, std::string> v = "hello";

std::visit([](auto&& arg) {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, int>)         std::cout << "int: " << arg;
    else if constexpr (std::is_same_v<T, double>) std::cout << "double: " << arg;
    else if constexpr (std::is_same_v<T, std::string>) std::cout << "string: " << arg;
}, v);
```

## 3. `if constexpr` (C++17) — Compile-Time Branch Elimination

Unlike regular `if`, `if constexpr` **discards the false branch at compile time** — the discarded branch doesn't even need to be valid for the given type:

```cpp
template <typename T>
auto process(T val) {
    if constexpr (std::is_integral_v<T>) {
        return val * 2;           // only compiled for integral types
    } else if constexpr (std::is_floating_point_v<T>) {
        return std::round(val);   // only compiled for floating-point types
    } else {
        static_assert(false, "unsupported type");  // compile error for other types
    }
}

// Without if constexpr, you'd need SFINAE or template specialization
// if constexpr makes this dramatically more readable
```

## 4. Three-Way Comparison `<=>` (C++20)

The spaceship operator generates **all six comparison operators** from a single declaration:

```cpp
struct Point {
    int x, y;
    auto operator<=>(const Point&) const = default;
    // Compiler generates: ==, !=, <, <=, >, >=
    // Comparison is lexicographic: first x, then y
};

// Return type categories:
// strong_ordering  — exactly one of {less, equal, greater} (for int, string)
// weak_ordering    — equivalent values may not be equal (case-insensitive string)
// partial_ordering — some values are incomparable (NaN for floating-point)

// Custom example:
struct CaseInsensitiveString {
    std::string s;
    std::weak_ordering operator<=>(const CaseInsensitiveString& other) const {
        return toLower(s) <=> toLower(other.s);  // delegates to string's <=>
    }
};
```

**Rewrite rules:** The compiler rewrites `a < b` to `(a <=> b) < 0`, and `a == b` to `(a <=> b) == 0`. You only need to define `operator<=>` and optionally `operator==` (for efficiency — `==` can short-circuit on size for strings).

## 5. `deducing this` (C++23)

Explicit object parameter replaces CRTP for recursive lambdas and eliminates const/non-const overload duplication:

```cpp
// Before C++23 — must write two overloads:
class Container {
    std::vector<int> data;
public:
    int& at(size_t i)       { return data[i]; }
    const int& at(size_t i) const { return data[i]; }  // duplicated logic!
};

// C++23 — single definition with deducing this:
class Container {
    std::vector<int> data;
public:
    template <typename Self>
    auto&& at(this Self&& self, size_t i) {
        return std::forward<Self>(self).data[i];
        // If called on const object → returns const int&
        // If called on non-const   → returns int&
        // If called on rvalue      → returns int&&
    }
};

// Recursive lambda (replaces CRTP):
auto fibonacci = [](this auto self, int n) -> int {
    if (n <= 1) return n;
    return self(n - 1) + self(n - 2);  // recursive call via self
};
```

## 6. C++20 Coroutines — Lazy Generators

```cpp
#include <coroutine>
#include <generator>  // C++23 std::generator

std::generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;         // suspends, returns 'a' to caller
        auto next = a + b;
        a = b;
        b = next;
    }
}

for (int val : fibonacci() | std::views::take(10)) {
    std::cout << val << " ";  // 0 1 1 2 3 5 8 13 21 34
}
```

Coroutines enable **lazy, on-demand computation** without manual state machine construction. The compiler transforms the function into a state machine with a heap-allocated **coroutine frame** that stores local variables across suspension points.
