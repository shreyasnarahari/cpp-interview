# Templates & Metaprogramming — Real Interview Questions

> No answers — attempt cold first.

---

## Theory Questions
1. What is a function template? How does template argument deduction work?
2. What is template specialization? What is partial specialization?
3. What is SFINAE? Why is it useful? Give an example.
4. What are variadic templates? What is a parameter pack?
5. What is `if constexpr` (C++17)? How is it different from regular `if`?
6. What are fold expressions (C++17)? Give an example.
7. What are C++20 Concepts? How do they replace SFINAE?
8. What is `std::enable_if`? How does it work?
9. What is `constexpr`? What can be `constexpr` in C++14 vs C++20?
10. What is the difference between `constexpr` and `consteval` (C++20)?
11. What are type traits? Name 5 commonly used ones.
12. What is tag dispatch? When would you use it?
13. What is `std::void_t`? How is it used for the detection idiom?
14. What is CTAD (Class Template Argument Deduction, C++17)?

## Code Questions

```cpp
// Q1: Which function is called?
template <typename T> void f(T) { std::cout << "template\n"; }
template <> void f<int>(int) { std::cout << "specialized\n"; }
void f(int) { std::cout << "non-template\n"; }

f(42);         // ?
f<int>(42);    // ?
f(3.14);       // ?
```

```cpp
// Q2: Does this compile?
template <typename T>
auto add(T a, T b) { return a + b; }

auto result = add(1, 2.0);  // ?
```

```cpp
// Q3: What does this print?
template <typename... Args>
auto sum(Args... args) {
    return (args + ...);  // fold expression
}
std::cout << sum(1, 2, 3, 4, 5);
```

## Implementation Problems
1. Implement compile-time factorial using template metaprogramming
2. Write a `print_all(args...)` variadic template that prints all arguments
3. Implement `std::is_same<T, U>` from scratch
4. Write a function that only accepts integral types using C++20 concepts
5. Implement a compile-time type list with `TypeList<int, double, char>`
