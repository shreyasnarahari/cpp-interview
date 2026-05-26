# Modern C++ — Easy Questions (C++11/14)

---

**Q1. What is `nullptr`?**

A: A type-safe null pointer constant (type `std::nullptr_t`) replacing `NULL` and `0`:

```cpp
void f(int) { std::cout << "int\n"; }
void f(int*) { std::cout << "pointer\n"; }

f(0);        // calls f(int) — ambiguous intent
f(NULL);     // might call f(int) — NULL is often #define 0
f(nullptr);  // calls f(int*) — unambiguous
```

---

**Q2. What is `enum class` (scoped enum)?**

A: Strongly-typed enumerations that don't leak names into the enclosing scope:

```cpp
// Old (leaky):
enum Color { Red, Green, Blue };
// int x = Red;  // Red is in global scope, implicit int conversion

// Modern (scoped):
enum class Color { Red, Green, Blue };
Color c = Color::Red;            // must qualify
// int x = Color::Red;           // ERROR — no implicit conversion
int x = static_cast<int>(Color::Red);  // explicit conversion OK

// Can specify underlying type:
enum class Status : uint8_t { OK = 0, Error = 1 };
```

---

**Q3. What are range-based for loops?**

A:
```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

// By value (copy):
for (int x : v) { /* x is a copy */ }

// By const reference (read-only, no copy):
for (const auto& x : v) { /* preferred for reading */ }

// By reference (modify in place):
for (auto& x : v) { x *= 2; }

// Works with arrays, initializer lists, and any type with begin()/end()
for (int x : {10, 20, 30}) { std::cout << x << " "; }
```

---

**Q4. What is `std::array`?**

A: A fixed-size array wrapper with value semantics (copyable, knows its size):

```cpp
std::array<int, 5> a = {1, 2, 3, 4, 5};
a.size();    // 5 — knows its size (raw arrays decay to pointers)
a.at(10);    // throws std::out_of_range (bounds-checked)
a[2];        // 3 — no bounds check, like raw array

// Can be passed by value/reference without decay:
void f(std::array<int, 5> arr);  // size is part of the type
```

---

**Q5. What is `auto` return type deduction (C++14)?**

A:
```cpp
auto add(int a, int b) {
    return a + b;  // compiler deduces return type as int
}

// With different return types — all branches must agree:
auto process(bool flag) {
    if (flag) return 42;
    return 0;  // OK — both int
    // return "hello"; // ERROR — different types
}

// decltype(auto) preserves references:
decltype(auto) getRef(std::vector<int>& v) {
    return v[0];  // returns int& (not int)
}
```

---

## Coding Questions

---

**C1. Modernize this C++03 code**

```cpp
// Before:
typedef std::vector<std::pair<std::string, int> > StringIntVec;
StringIntVec data;
for (StringIntVec::iterator it = data.begin(); it != data.end(); ++it) {
    std::cout << it->first << ": " << it->second << std::endl;
}
```

A:
```cpp
// After (C++11/14):
using StringIntVec = std::vector<std::pair<std::string, int>>;
StringIntVec data;
for (const auto& [name, value] : data) {  // structured bindings (C++17)
    std::cout << name << ": " << value << '\n';
}
```

---

**C2. Init capture to move into lambda**

A:
```cpp
auto ptr = std::make_unique<Widget>();
// auto fn = [ptr]() {};  // ERROR — can't copy unique_ptr
auto fn = [p = std::move(ptr)]() {
    p->doWork();
};
```

---

**C3. `constexpr` with loops (C++14)**

A:
```cpp
constexpr int sum_to(int n) {
    int total = 0;
    for (int i = 1; i <= n; i++) total += i;
    return total;
}
static_assert(sum_to(10) == 55);  // computed at compile time
```
