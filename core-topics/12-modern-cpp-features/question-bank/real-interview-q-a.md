# Modern C++ Features — Real Interview Questions

> No answers — attempt cold first.

---

## "What's new in C++11?"
1. What are rvalue references and move semantics?
2. What is `auto`? What are its deduction rules?
3. What are smart pointers (`unique_ptr`, `shared_ptr`, `weak_ptr`)?
4. What are lambda expressions?
5. What is `constexpr`?
6. What are range-based for loops?
7. What is `nullptr`? Why not just use `0` or `NULL`?
8. What is `std::thread` and the concurrency library?
9. What is `enum class` (scoped enumerations)?
10. What is `std::array`?

## "What's new in C++14?"
1. What are generic lambdas (`auto` parameters)?
2. What are init captures (generalized lambda captures)?
3. What is `std::make_unique`?
4. What is return type deduction for functions?
5. What is relaxed `constexpr` (loops and local variables)?

## "What's new in C++17?"
1. What are structured bindings?
2. What is `if constexpr`?
3. What is `std::optional`?
4. What is `std::variant` and `std::visit`?
5. What is `std::any`?
6. What is `std::string_view`?
7. What are fold expressions?
8. What is CTAD (Class Template Argument Deduction)?
9. What is `if`/`switch` with initializer?
10. What is `[[nodiscard]]`?
11. What are nested namespaces (`namespace A::B::C`)?

## "What's new in C++20?"
1. What are Concepts?
2. What are Ranges?
3. What are Coroutines?
4. What are Modules?
5. What is the three-way comparison operator (`<=>`)?
6. What is `consteval`? `constinit`?
7. What is `std::span`?
8. What is `std::jthread`?
9. What are `std::format` and formatted output?
10. What is the `requires` clause?

## "What's new in C++23?"
1. What is `std::expected`?
2. What is deducing `this`?
3. What is `std::print`?
4. What are `std::flat_map` and `std::flat_set`?
5. What are `std::generator` and other coroutine utilities?

## Code Questions

```cpp
// Q1: What does this C++17 code do?
auto [name, age] = std::make_pair("Alice", 30);
```

```cpp
// Q2: What does this C++20 code do?
auto result = std::views::iota(1, 100)
    | std::views::filter([](int x) { return x % 3 == 0; })
    | std::views::transform([](int x) { return x * x; })
    | std::views::take(5);
```

```cpp
// Q3: What does <=> return?
struct Point {
    int x, y;
    auto operator<=>(const Point&) const = default;
};
```
