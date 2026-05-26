# Const Correctness & Type System — Real Interview Questions

> No answers — attempt cold first, then check easy.md / medium.md / hard.md.

---

## Theory Questions

1. What is the difference between `const int*`, `int* const`, and `const int* const`?
2. What is a const member function? Why should you use it?
3. What is the `mutable` keyword? Give a real-world use case.
4. What is the difference between `const` and `constexpr`?
5. What is `consteval` (C++20)? How is it different from `constexpr`?
6. What is `constinit` (C++20)? What problem does it solve?
7. What is `volatile`? Is it useful for multithreading? (no)
8. What is const overloading? Give an example with `operator[]`.
9. Explain the four C++ casting operators: `static_cast`, `dynamic_cast`, `const_cast`, `reinterpret_cast`.
10. When is `const_cast` safe? When is it undefined behavior?
11. What does `dynamic_cast` need to work? What happens if it fails?
12. Why should you avoid C-style casts in C++?
13. What is the "east const" vs "west const" debate?
14. Can a `const` method call a non-const method?
15. What is the difference between top-level const and low-level const?

---

## "What does this print?" Code Questions

```cpp
// Q1:
const int ci = 42;
int* p = const_cast<int*>(&ci);
*p = 100;
std::cout << ci << " " << *p;
```

```cpp
// Q2:
class Cache {
    mutable int hits_ = 0;
public:
    int get() const { hits_++; return 42; }
    int getHits() const { return hits_; }
};
const Cache c;
c.get(); c.get();
std::cout << c.getHits();
```

```cpp
// Q3:
class Widget {
    int x_ = 10;
public:
    int& get() { std::cout << "non-const "; return x_; }
    const int& get() const { std::cout << "const "; return x_; }
};
Widget w;
const Widget& cw = w;
w.get();
cw.get();
```

```cpp
// Q4:
constexpr int square(int x) { return x * x; }
int y = 5;
constexpr int a = square(3);   // ?
int b = square(y);             // ?
// constexpr int c = square(y); // ?
```

```cpp
// Q5:
struct Base { virtual ~Base() {} };
struct Derived : Base { int data = 42; };
Base* bp = new Base();
Derived* dp = dynamic_cast<Derived*>(bp);
std::cout << (dp == nullptr ? "null" : "valid");
```

---

## Implementation Problems

1. Write a class `Matrix` with const-correct `operator()` (both const and non-const versions).
2. Implement a thread-safe cache using `mutable std::mutex` in const methods.
3. Write a compile-time lookup table using `constexpr`.
4. Demonstrate all four cast operators with correct and incorrect usage examples.
