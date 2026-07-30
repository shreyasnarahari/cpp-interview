# Theory & Mechanics: Standards Chronology

## 1. Timeline of Modern C++ Standards

```
  C++03 ----> C++11 ----> C++14 ----> C++17 ----> C++20 ----> C++23
(Legacy)    (Modern)    (Tweak)    (Utility)  (Major)    (Polish)
```

## 2. Key Paradigm Transformations

- **C++11**: Value categories formalized; move constructors enable zero-copy transfers.
- **C++17**: Class Template Argument Deduction (CTAD), Structured Bindings (`auto [a, b]`), `if constexpr` compile-time branches.
- **C++20**: Concepts replace SFINAE; Ranges provide composable pipeline syntax; Coroutines enable async await loops.
- **C++23**: `deducing this` replaces CRTP for recursive lambdas and const/non-const overload duplication.

```cpp
// C++23 Deducing This (explicit object parameter)
struct Builder {
    template <typename Self>
    Self&& set_id(this Self&& self, int id) {
        self.id = id;
        return std::forward<Self>(self);
    }
    int id;
};
```
