# Theory & Mechanics: Modern C++ Features (C++11 through C++23)

## 1. Feature Chronology Matrix

```
                       ISO C++ Standards Evolution:
  ┌──────────┬──────────┬──────────┬───────────────────────┬──────────────────────┐
  │  C++11   │  C++14   │  C++17   │        C++20          │        C++23         │
  ├──────────┼──────────┼──────────┼───────────────────────┼──────────────────────┤
  │ Move     │ Generic  │ Struct   │ Concepts, Ranges,     │ std::expected,       │
  │ Semantics│ Lambdas, │ Bindings,│ Coroutines, Modules,  │ deducing this,       │
  │ SmartPtrs│ Init     │ Optional,│ Spaceship (<=>),      │ std::print,          │
  │ Lambdas  │ Captures,│ Variant, │ consteval, std::span, │ std::flat_map,       │
  │ constexpr│ make_    │ String_  │ std::format,          │ std::generator       │
  │ nullptr  │ unique   │ View     │ std::jthread          │                      │
  └──────────┴──────────┴──────────┴───────────────────────┴──────────────────────┘
```

---

## 2. Deep Dive: C++20 Spaceship Operator (`<=>`)

The Three-Way Comparison Operator (`<=>`) evaluates ordering between two objects in a single operation:

```cpp
#include <compare>

struct Point {
    int x, y;
    auto operator<=>(const Point&) const = default; // Auto-generates all 6 comparison operators!
};
```

### Ordering Return Categories:
1. **`std::strong_ordering`**: Strict total ordering (`less`, `equal`, `greater`). Substitutability holds (if $a == b$, then $f(a) == f(b)$). Used for integers and pointers.
2. **`std::weak_ordering`**: Equivalent but not equal (e.g. case-insensitive string comparison `"Hello" == "hello"`).
3. **`std::partial_ordering`**: Some values are incomparable (`unordered`). Used for floating-point numbers (`NaN <=> 1.0` returns `partial_ordering::unordered`).

---

## 3. Deep Dive: C++20 Coroutines Mechanics

A C++20 Coroutine is a function that can **suspend execution** and **resume later** while preserving its internal stack frame.

### The 3 Coroutine Keywords:
- `co_await expr`: Suspends execution until an asynchronous operation completes.
- `co_yield expr`: Returns a value to the caller and suspends execution (Generators).
- `co_return expr`: Terminates execution and returns a final value.

### Internal Coroutine Frame Allocation:
When a coroutine is called, it allocates a **Coroutine Frame** on the heap (or optimized away via HALO - Heap Allocation Removal Optimization):

```
Coroutine Frame Anatomy (Heap Allocation):
┌───────────────────────────────────────────────┐
│ Promise Object (Manages return value/errors) │
├───────────────────────────────────────────────┤
│ Parameter Copies                              │
├───────────────────────────────────────────────┤
│ Local Variables                               │
├───────────────────────────────────────────────┤
│ Suspend / Resume Instruction Pointer State    │
└───────────────────────────────────────────────┘
```

---

## 4. Deep Dive: C++20 Ranges & Lazy Views

C++20 Ranges pipeline operations lazily using composable pipe syntax (`|`), performing $0$ intermediate allocations:

```cpp
#include <ranges>
#include <vector>
#include <iostream>

int main() {
    std::vector<int> nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Lazy evaluation: No elements are processed until iterated!
    auto result = nums 
        | std::views::filter([](int x) { return x % 2 == 0; })
        | std::views::transform([](int x) { return x * x; })
        | std::views::take(3);

    for (int v : result) {
        std::cout << v << " "; // PRINTS: 4 16 36
    }
}
```

---

## 5. Vocabulary Types: `std::optional`, `std::variant`, `std::any`

### 1. `std::optional<T>` (C++17)
Represents a value that may or may not exist without heap allocation (uses `alignas(T) char storage[sizeof(T)]` inside).

### 2. `std::variant<Ts...>` (C++17)
Type-safe, non-allocating tagged union. Accessed via `std::get<T>(v)` or pattern-matched via `std::visit(visitor, v)`.

### 3. `std::any` (C++17)
Type-erased container that can hold ANY copyable type. Uses Small Buffer Optimization; allocates heap memory for large objects. Extracted via `std::any_cast<T>(a)`.

---

## 6. C++23 Breakthroughs: Monadic Error & `deducing this`

### 1. Monadic Error Handling (`std::expected<T, E>`)
Stores either a success value `T` or an error `E` without heap allocation or exception unwinding:
```cpp
std::expected<int, std::string> parse(std::string_view s);
auto result = parse("123").transform([](int v) { return v * 2; });
```

### 2. Explicit Object Parameters (`deducing this`)
Replaces CRTP and eliminates const/non-const method duplication:
```cpp
struct Node {
    template <typename Self>
    auto&& get_value(this Self&& self) {
        return std::forward<Self>(self).value_; // Deduces value category of caller!
    }
    int value_;
};
```
