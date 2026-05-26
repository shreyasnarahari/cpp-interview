# Exception Handling & noexcept — Real Interview Questions

> No answers — attempt cold first, then check easy.md / medium.md / hard.md.

---

## Theory Questions

1. What are the three exception safety guarantees? Define each.
2. What is `noexcept`? Why should you use it on move operations?
3. What happens if a `noexcept` function throws? (std::terminate)
4. What is stack unwinding? What happens to local objects during unwinding?
5. Why should you catch exceptions by const reference?
6. What is the difference between `throw;` and `throw e;` in a catch block?
7. Is it safe to throw an exception in a destructor? Why or why not?
8. What is the `noexcept` operator (not specifier)? How do you use it?
9. What is `std::exception_ptr`? When would you use it?
10. What is the difference between exceptions and error codes? When to use each?
11. How does RAII interact with exception safety?
12. What is `std::nested_exception`?
13. What happened to `throw()` and dynamic exception specifications?
14. How does `std::vector` use `noexcept` to decide between copy and move during reallocation?

---

## "What happens?" Code Questions

```cpp
// Q1:
void f() noexcept {
    throw std::runtime_error("oops");
}
int main() { f(); }
```

```cpp
// Q2:
struct Widget {
    ~Widget() noexcept(false) { throw std::runtime_error("dtor"); }
};
void f() {
    try {
        Widget w;
        throw std::runtime_error("body");
        // ~Widget called during stack unwinding — throws again
    } catch (...) {
        std::cout << "caught";
    }
}
```

```cpp
// Q3:
struct Base {
    virtual ~Base() = default;
};
struct Derived : Base {};

try {
    throw Derived();
} catch (Base e) {       // by VALUE — slicing!
    throw;               // what is re-thrown?
}
```

```cpp
// Q4:
void process() {
    int* data = new int[1000];
    riskyOperation();    // might throw
    delete[] data;       // never reached if exception
}
```

---

## Implementation Problems

1. Write an exception-safe `push_back` for a custom dynamic array (strong guarantee).
2. Implement a function that transfers elements between two containers with rollback on failure.
3. Write a custom exception hierarchy: `AppError` → `NetworkError` / `DatabaseError`.
4. Show how `noexcept(false)` on a move constructor changes `std::vector` behavior.
