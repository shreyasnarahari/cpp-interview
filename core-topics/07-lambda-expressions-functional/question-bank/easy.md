# Lambda Expressions — Easy Questions

---

**Q1. What is a lambda?**

A: An anonymous function object (closure). Syntax:
```cpp
[captures](parameters) -> return_type { body }

auto add = [](int a, int b) { return a + b; };
std::cout << add(3, 4);  // 7
```

The compiler generates a unique, unnamed class with `operator()`.

---

**Q2. What are capture modes?**

A:
```cpp
int x = 10, y = 20;

auto f1 = [x]() { return x; };         // capture x by value
auto f2 = [&x]() { x++; };             // capture x by reference
auto f3 = [=]() { return x + y; };     // capture ALL by value
auto f4 = [&]() { x++; y++; };         // capture ALL by reference
auto f5 = [x, &y]() { y = x; };       // x by value, y by reference
auto f6 = [this]() { member_fn(); };   // capture this pointer
auto f7 = [*this]() { /* ... */ };     // capture copy of *this (C++17)
```

---

**Q3. What is a mutable lambda?**

A: By default, a lambda's `operator()` is `const` — captured-by-value variables can't be modified. `mutable` removes the const:

```cpp
int x = 10;
auto fn = [x]() mutable {
    x += 5;  // OK — mutable allows modification of the copy
    return x;
};
fn();  // returns 15
fn();  // returns 20 (x persists between calls!)
// Original x is still 10
```

---

**Q4. What is `std::function`?**

A: A type-erased callable wrapper. Can store any callable with matching signature:

```cpp
std::function<int(int, int)> fn;

fn = [](int a, int b) { return a + b; };  // lambda
fn = std::plus<int>{};                     // function object
fn = &freeFunction;                        // function pointer

// Overhead: heap allocation + virtual dispatch (~30ns vs ~1ns for direct call)
// Use template parameters when possible for zero overhead
```

---

**Q5. What is a generic lambda (C++14)?**

A: A lambda with `auto` parameters — implicitly a template:

```cpp
auto print = [](const auto& val) { std::cout << val << "\n"; };
print(42);       // int
print("hello");  // const char*
print(3.14);     // double

// Equivalent to:
struct {
    template <typename T>
    void operator()(const T& val) const { std::cout << val << "\n"; }
} print;
```

---

## Coding Questions

---

**C1. Sort with custom comparator**

A:
```cpp
std::vector<std::pair<std::string, int>> items = {
    {"apple", 3}, {"banana", 1}, {"cherry", 2}
};
std::sort(items.begin(), items.end(),
    [](const auto& a, const auto& b) { return a.second < b.second; });
// Sorted by count: banana(1), cherry(2), apple(3)
```

---

**C2. Immediately Invoked Lambda Expression (IILE)**

A:
```cpp
// Use IILE for complex const initialization:
const auto config = [&]() {
    Config c;
    c.host = getEnv("HOST");
    c.port = std::stoi(getEnv("PORT"));
    c.debug = getEnv("DEBUG") == "1";
    return c;
}();  // immediately invoked!
// config is const and fully initialized
```

---

**C3. Transform with lambda**

A:
```cpp
std::vector<int> nums = {1, 2, 3, 4, 5};
std::vector<int> squares;
std::transform(nums.begin(), nums.end(), std::back_inserter(squares),
    [](int x) { return x * x; });
// squares = {1, 4, 9, 16, 25}
```

---

**C4. Capture unique_ptr with init capture (C++14)**

A:
```cpp
auto ptr = std::make_unique<Widget>();
// auto fn = [ptr]() { ... };        // ERROR — unique_ptr not copyable

auto fn = [p = std::move(ptr)]() {   // init capture — move into lambda
    p->doWork();
};
fn();
// ptr is now nullptr (moved into lambda)
```
