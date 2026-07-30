# Theory & Mechanics: Move Semantics & Value Categories

## 1. Value Category Taxonomy

Every C++ expression has two orthogonal properties: **identity** (can you take its address?) and **movability** (can resources be stolen from it?).

```
             has identity?
                yes              no
           ┌────────────┬────────────┐
can move?  │            │            │
   no      │  lvalue    │  (unused)  │
           │            │            │
   yes     │  xvalue    │  prvalue   │
           │            │            │
           └────────────┴────────────┘

Composite categories:
  glvalue = lvalue | xvalue    (has identity)
  rvalue  = xvalue | prvalue   (can be moved from)
```

- **lvalue**: `x`, `*ptr`, `arr[0]`, `"hello"` — persists beyond the expression
- **prvalue**: `42`, `x + y`, `Widget()` — temporary, no identity
- **xvalue**: `std::move(x)`, `static_cast<T&&>(x)` — has identity but is expiring

## 2. `std::move` — It's Just a Cast

`std::move` does NOT move anything. It unconditionally casts its argument to an rvalue reference:

```cpp
// Simplified implementation:
template <typename T>
constexpr std::remove_reference_t<T>&& move(T&& arg) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(arg);
}

// What happens:
std::string s = "hello";
std::string s2 = std::move(s);
// 1. std::move(s) → static_cast<std::string&&>(s)  — just a cast, zero cost
// 2. This rvalue ref binds to string's MOVE constructor
// 3. The move ctor steals s's internal buffer (pointer swap)
// 4. s is now in a "valid but unspecified" state (typically empty)
```

**Critical rule:** A named rvalue reference is itself an **lvalue** (it has a name):
```cpp
void process(std::string&& s) {
    std::string local = s;            // COPIES! s is an lvalue here
    std::string local2 = std::move(s); // moves — std::move makes it xvalue again
}
```

## 3. Reference Collapsing Rules

When references-to-references form during template deduction, they collapse:

```
T&   &   → T&      (lvalue ref to lvalue ref → lvalue ref)
T&   &&  → T&      (lvalue ref to rvalue ref → lvalue ref)
T&&  &   → T&      (rvalue ref to lvalue ref → lvalue ref)
T&&  &&  → T&&     (rvalue ref to rvalue ref → rvalue ref)
```

**Rule of thumb:** If any `&` appears, the result is `&`. Only `&& &&` yields `&&`.

This is the mechanism behind **forwarding references** (`T&&` in template context):
```cpp
template <typename T>
void wrapper(T&& arg);

int x = 42;
wrapper(x);    // T = int&,  T&& = int& && = int&   (lvalue → lvalue ref)
wrapper(42);   // T = int,   T&& = int&&             (rvalue → rvalue ref)
```

## 4. Perfect Forwarding

`std::forward<T>` preserves the original value category:

```cpp
template <typename T>
void wrapper(T&& arg) {
    target(std::forward<T>(arg));
}

// When called with lvalue (T = int&):
//   std::forward<int&>(arg) → static_cast<int& &&>(arg) → static_cast<int&>(arg)
//   → passes as lvalue ✓

// When called with rvalue (T = int):
//   std::forward<int>(arg) → static_cast<int&&>(arg)
//   → passes as rvalue ✓
```

Without `std::forward`, the named parameter `arg` would ALWAYS be passed as an lvalue (because it has a name), losing the rvalue-ness of the original argument.

## 5. RVO & NRVO — Copy Elision

**RVO (Return Value Optimization):** When returning a prvalue, the compiler constructs the object directly in the caller's space:

```cpp
Widget create() {
    return Widget(42);  // prvalue — RVO guaranteed since C++17
}
Widget w = create();
// Only ONE constructor call total — no copy, no move
```

**NRVO (Named Return Value Optimization):** When returning a named local:
```cpp
Widget create() {
    Widget w(42);    // named local
    return w;        // NRVO — compiler MAY elide the move
}
// NRVO is NOT guaranteed — it's an optimization the compiler is permitted to do
```

**Anti-pattern — `std::move` on return PREVENTS NRVO:**
```cpp
Widget create() {
    Widget w(42);
    return std::move(w);  // BAD — forces move, prevents NRVO
    // return w;           // GOOD — allows NRVO, falls back to implicit move
}
```

## 6. `noexcept` and Move — The `std::vector` Connection

`std::vector::push_back` during reallocation must maintain the **strong exception guarantee** (if it fails, the original state is preserved). It uses `std::move_if_noexcept`:

```cpp
// If move ctor is noexcept → elements are MOVED (fast, O(n) pointer swaps)
// If move ctor might throw → elements are COPIED (safe, preserves original on failure)

class Widget {
public:
    Widget(Widget&&) noexcept;  // mark noexcept → vector will MOVE during realloc
};
```

This is why **every move constructor/assignment should be `noexcept`** — without it, `std::vector` falls back to copying, silently destroying performance.
