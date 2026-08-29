# Move Semantics & Value Categories — Deep Theory & Mechanics

An exhaustive, low-level reference on C++11/C++17/C++20 value categories, rvalue references, universal/forwarding references, reference collapsing rules, perfect forwarding, `std::move` vs `std::forward` assembly mechanics, copy elision (RVO/NRVO), and `noexcept` reallocation invariants.

---

## 1. The C++ Value Category Taxonomy (C++11 to C++23)

In C++03, expressions were simply "lvalues" or "rvalues". In modern C++ (C++11 onwards), every expression is characterized by **two independent fundamental properties**:
1. **Has Identity**: Does the program have an identifiable memory address for the object?
2. **Can Be Moved From**: Can the resources of the object be safely transferred/pilfered?

```
                               EXPRESSIONS
                             /             \
                            /               \
            [HAS IDENTITY] /                 \ [CAN BE MOVED]
                          /                   \
                         v                     v
                 +---------------+     +---------------+
                 |    glvalue    |     |    rvalue     |
                 | (Generalized) |     |  (Right-Value)|
                 +---------------+     +---------------+
                    /         \           /         \
                   /           \         /           \
                  v             v       v             v
          +---------------+  +---------------+  +---------------+
          |    lvalue     |  |    xvalue     |  |    prvalue    |
          | (Left-Value)  |  |  (eXpiring)   |  | (Pure Rvalue) |
          +---------------+  +---------------+  +---------------+
          [Has Identity]     [Has Identity]     [No Identity]
          [Cannot Move]      [Can Be Moved]     [Can Be Moved]
```

### 1.1 Category Definitions & Concrete Examples

| Category | Definition | Concrete Examples |
|---|---|---|
| **lvalue** | Has identity; cannot be moved implicitly. | Named variables (`x`, `str`), function calls returning lvalue reference (`arr[i]`, `++x`), string literals (`"hello"`). |
| **prvalue** | Pure rvalue; computes a value that has no identity until materialized. | Literals (`42`, `3.14`), non-reference return values (`f()`), arithmetic results (`a + b`), lambdas (`[](){}`). |
| **xvalue** | eXpiring value; has identity but its lifetime is ending and its resources can be pilfered. | Cast to rvalue reference (`static_cast<T&&>(x)`, `std::move(x)`), member access on rvalue (`std::move(p).member`). |
| **glvalue** | Generalized lvalue (`lvalue \cup xvalue`). | Any expression with an identifiable memory address. |
| **rvalue** | Moveable value (`prvalue \cup xvalue`). | Any expression that can bind to an rvalue reference (`T&&`). |

---

## 2. Forwarding References & Reference Collapsing

### 2.1 Universal (Forwarding) References
A reference `T&&` is a **Forwarding Reference** if and only if it involves type deduction:
1. Function template parameter: `template <typename T> void f(T&& param);`
2. Auto deduction: `auto&& var = expr;`

*Note: In `template <typename T> class Vector { void push_back(T&& x); };`, `T&&` is a regular **rvalue reference**, NOT a forwarding reference, because `T` was already deduced when the class was instantiated!*

### 2.2 The 4 Reference Collapsing Rules
When template deduction attempts to create a "reference to a reference", C++ applies **Reference Collapsing**:

$$\& + \& \implies \&$$
$$\& + \&\& \implies \&$$
$$\&\& + \& \implies \&$$
$$\&\& + \&\& \implies \&\&$$

**Golden Rule**: If either reference is an lvalue reference (`&`), the result collapses to an **lvalue reference (`&`)**. It collapses to an rvalue reference (`&&`) **only if both** are rvalue references!

```cpp
template <typename T>
void wrapper(T&& arg);

int x = 10;
wrapper(x);  // x is lvalue -> T deduced as `int&`  -> `int& &&` collapses to `int&`
wrapper(10); // 10 is rvalue -> T deduced as `int`   -> `int&&` is `int&&`
```

---

## 3. `std::move` vs `std::forward`: Assembly & Implementation

Neither `std::move` nor `std::forward` executes any CPU instructions at runtime! They are purely **compile-time static casts**.

### 3.1 Custom Implementation of `std::move`
Unconditionally casts any expression into an rvalue (`xvalue`):

```cpp
template <typename T>
[[nodiscard]] constexpr std::remove_reference_t<T>&& custom_move(T&& arg) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(arg);
}
```

### 3.2 Custom Implementation of `std::forward`
Conditionally casts an expression to an rvalue **only if** the original template argument was passed as an rvalue:

```cpp
// 1. Forwarding lvalues as lvalues or rvalues
template <typename T>
[[nodiscard]] constexpr T&& custom_forward(std::remove_reference_t<T>& arg) noexcept {
    return static_cast<T&&>(arg);
}

// 2. Forwarding rvalues (prevents forwarding temporary as lvalue)
template <typename T>
[[nodiscard]] constexpr T&& custom_forward(std::remove_reference_t<T>&& arg) noexcept {
    static_assert(!std::is_lvalue_reference_v<T>, "Cannot forward rvalue as lvalue");
    return static_cast<T&&>(arg);
}
```

---

## 4. Return Value Optimization (RVO) & Copy Elision

### 4.1 Unnamed RVO (URVO) — Mandatory in C++17
When returning a temporary (prvalue), C++17 guarantees **zero copy and zero move constructions**! The object is constructed directly into the caller's stack frame storage.

```cpp
Widget create_widget() {
    return Widget(42); // Guaranteed Zero-Copy / Zero-Move in C++17!
}
Widget w = create_widget(); // Constructed directly in w's memory location!
```

### 4.2 Named Return Value Optimization (NRVO)
When returning a named local variable:

```cpp
Widget create_widget() {
    Widget w(42);
    w.modify();
    return w; // NRVO: Compiler constructs 'w' directly in caller's return slot
}
```

### 4.3 The "Pessimizing `std::move`" Anti-Pattern
Calling `return std::move(w);` **prevents RVO/NRVO**! It forces the compiler to treat `w` as an xvalue, turning a zero-overhead elision into a move construction:

```cpp
Widget get() {
    Widget w;
    return std::move(w); // BAD! Disables NRVO elision; forces move construction!
}
```

---

## 5. `noexcept` on Move Operations & `std::vector` Invariant

When `std::vector` reallocates its internal buffer to grow capacity:
- It must preserve the **Strong Exception Guarantee** (if copying/moving throws, the original vector elements must remain intact).
- If `T` has a **non-`noexcept` move constructor**, moving an element halfway through reallocation might throw an exception, corrupting the original vector!
- Therefore, `std::vector` uses `std::move_if_noexcept`:
  - If `is_nothrow_move_constructible_v<T> == true` $\implies$ **Fast Move ($O(1)$ pointer transfer)**.
  - If `is_nothrow_move_constructible_v<T> == false` $\implies$ **Slow Deep Copy ($O(N)$ memory allocations)**!

```cpp
class HeavyPayload {
    char* data;
    size_t size;
public:
    // ALWAYS mark move constructor and move assignment NOEXCEPT!
    HeavyPayload(HeavyPayload&& other) noexcept 
        : data(std::exchange(other.data, nullptr)), size(std::exchange(other.size, 0)) {}

    HeavyPayload& operator=(HeavyPayload&& other) noexcept {
        if (this != &other) {
            delete[] data;
            data = std::exchange(other.data, nullptr);
            size = std::exchange(other.size, 0);
        }
        return *this;
    }
};
```

---

## 6. Moved-From State Invariants

According to the ISO C++ Standard (§ [lib.types.movedfrom]):
- A moved-from object must be left in a **"valid but unspecified state"**.
- Invariants:
  1. It can be safely destroyed (`~T()`).
  2. It can be assigned to (`operator=`).
  3. Precondition-free queries can be called (`.empty()`, `.size() == 0`).
  4. Accessing elements without checking bounds is undefined behavior.

---

## 7. Member Function Reference Qualifiers

Member functions can be overloaded based on whether the calling object is an lvalue (`&`) or an rvalue (`&&`):

```cpp
class DataStore {
    std::vector<std::string> cache_;
public:
    // Called when *this is an lvalue: returns const reference (avoids copy)
    const std::vector<std::string>& get_data() const & {
        return cache_;
    }

    // Called when *this is an rvalue (temporary): MOVES internal buffer directly!
    std::vector<std::string> get_data() && {
        return std::move(cache_);
    }
};

DataStore ds;
auto a = ds.get_data();           // Calls `&` version -> Copies cache_
auto b = DataStore().get_data();  // Calls `&&` version -> MOVES cache_ (Zero Copy!)
```
