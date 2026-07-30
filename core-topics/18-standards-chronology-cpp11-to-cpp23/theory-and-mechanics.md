# Theory & Mechanics: Standards Chronology (C++11 to C++23)

## 1. Evolution Matrix of Modern C++ Standards

```
Standard   Primary Milestone                     Key Features Introduced
─────────────────────────────────────────────────────────────────────────────────────────────────────
C++11      Modern C++ Foundation                 Move Semantics, Smart Pointers, Lambdas, auto,
                                                 constexpr, nullptr, enum class, std::thread, RAII
C++14      Language Refinements                  Generic Lambdas, Init Captures, std::make_unique,
                                                 Relaxed constexpr, Return Type Deduction
C++17      Ergonomics & Vocabulary Types         Structured Bindings (auto [a,b]), if constexpr,
                                                 std::optional, std::variant, std::string_view, CTAD
C++20      Language Paradigm Shift               Concepts, Ranges, Coroutines, Modules,
                                                 Spaceship Operator (<=>), consteval, std::span
C++23      Monadic Utilities & Explicit Object   std::expected, deducing this, std::print,
                                                 std::flat_map, std::generator
```

---

## 2. C++23 Feature Spotlight: Explicit Object Parameters (`deducing this`)

`deducing this` allows declaring member functions where the first parameter explicitly specifies `this` with a deduced template type (`this Self&& self`).

### 1. Eliminating Const/Non-Const Method Duplication
```cpp
class DataBuffer {
    std::vector<int> data_;
public:
    // Single method handles BOTH const and non-const calls without duplication!
    template <typename Self>
    auto&& get_data(this Self&& self) {
        return std::forward<Self>(self).data_;
    }
};
```

### 2. Replacing CRTP Static Polymorphism
```cpp
struct Base {
    template <typename Self>
    void print_name(this const Self& self) {
        std::cout << self.name() << "\n"; // Calls derived class implementation!
    }
};

struct Derived : Base {
    std::string name() const { return "Derived"; }
};
```

### 3. Recursive Lambdas Without `std::function`
```cpp
auto fib = [](this auto self, int n) -> int {
    if (n <= 1) return n;
    return self(n - 1) + self(n - 2); // Self-recursive call!
};
```
