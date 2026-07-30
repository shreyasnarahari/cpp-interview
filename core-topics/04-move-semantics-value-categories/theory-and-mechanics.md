# Theory & Mechanics: Move Semantics & Value Categories

## 1. Formal Value Categories Taxonomy

Every expression in C++ is characterized by two independent properties:
1. **Has Identity**: The program holds a memory address (`&expr` is valid) to identify the object.
2. **Can be Moved From**: The object can be bound to an rvalue reference (`T&&`) and its resources transferred.

```
                  Expressions (All C++ Expressions)
                             /         \
                            /           \
                           /             \
                  glvalue                 rvalue
                (has identity)         (can be moved)
                 /          \         /          \
                /            \       /            \
           lvalue             xvalue             prvalue
       (has identity,     (has identity,      (no identity,
       cannot move)        can move)           can move)
```

### Complete Breakdown of Categories:

| Category | Identity? | Movable? | Definition & Examples |
|---|---|---|---|
| **`lvalue`** | **Yes** | No | Functions, variables (`int x`), named rvalue refs (`std::string&& s`), string literals (`"hello"`). |
| **`prvalue`** | No | **Yes** | Literals (`42`, `3.14`), temporary objects (`Widget()`), lambda expressions, arithmetic expressions (`a + b`). |
| **`xvalue`** | **Yes** | **Yes** | Expiring values: Result of `std::move(x)`, member access of an rvalue (`Widget().data`). |
| **`glvalue`** | **Yes** | — | Generalized lvalue: Union of `lvalue` and `xvalue`. |
| **`rvalue`** | — | **Yes** | Union of `prvalue` and `xvalue`. Expressions that can bind to `const T&` or `T&&`. |

### CRITICAL TRAP: Named Rvalue References are Lvalues!
```cpp
void process(std::string&& s) {
    // 's' has type 'rvalue reference to string', BUT 's' has a name!
    // Therefore, the expression 's' is an LVALUE!
    std::string local1 = s;            // COPY constructor called!
    std::string local2 = std::move(s); // MOVE constructor called!
}
```

---

## 2. Under the Hood: What `std::move` Actually Does

`std::move` **does NOT move anything**, generate machine instructions, or touch object bytes at runtime! It is purely an **unconditional compile-time static cast** to an rvalue reference (`T&&` / `xvalue`).

### Implementation of `std::move`:
```cpp
template <typename T>
constexpr std::remove_reference_t<T>&& move(T&& arg) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(arg);
}
```

### Generated Assembly:
In optimized assembly (`-O2` / `-O3`), `std::move` evaporates entirely into zero machine instructions ($0$ cycles).

---

## 3. Moved-From State Semantics & `noexcept` Vector Growth

### Moved-From State Specification
According to the C++ Standard Library, a moved-from object must be left in a **valid but unspecified state**:
- **Valid**: Destructor can execute safely; member functions without preconditions (like `.clear()` or `operator=`) work cleanly.
- **Unspecified**: The exact value stored inside the object is implementation-dependent (usually null pointers or 0 sizes).

### Why Move Constructors MUST be Marked `noexcept`
`std::vector` growth (`push_back` / `reallocate`) requires the **Strong Exception Guarantee**:
1. When a vector reallocates its internal buffer, it transfers existing elements to the new buffer.
2. If it moves elements using move constructors, and element 5's move constructor **throws an exception**, the vector cannot roll back (elements 0..4 are already moved/modified!).
3. To maintain the Strong Guarantee, `std::vector` uses `std::move_if_noexcept<T>`:
   - If `T` has a `noexcept` move constructor $\rightarrow$ **Uses Move Constructor**.
   - If `T` lacks `noexcept` (or is copy-only) $\rightarrow$ **Falls back to Copy Constructor** (slower!).

```cpp
struct Element {
    Element(Element&&) noexcept; // CRITICAL: Enables O(1) vector reallocations!
};
```

---

## 4. Perfect Forwarding & `std::forward`

### Forwarding References (Universal References)
A syntax `T&&` is a **Forwarding Reference** if and only if:
1. `T` is a **deduced template parameter** of the function being called.
2. The type is written exactly as `T&&` without `const` or `volatile` qualifiers.

```cpp
template <typename T>
void f(T&& arg); // Forwarding reference (deduced)

void g(Widget&& arg); // Rvalue reference (concrete type, NOT forwarding reference!)

template <typename T>
void h(const T&& arg); // Rvalue reference (has const, NOT forwarding reference!)
```

### Reference Collapsing Rules Matrix
When binding arguments to a forwarding reference `T&&`, the compiler applies **Reference Collapsing**:

| Argument Passed | Deduced `T` | Parameter `T&&` Expands To | Collapses To |
|---|---|---|---|
| **Lvalue** `Widget w; f(w)` | `Widget&` | `Widget& &&` | **`Widget&` (lvalue ref)** |
| **Rvalue** `f(Widget())` | `Widget` | `Widget&&` | **`Widget&&` (rvalue ref)** |

Reference Collapsing Rules:
- `&` + `&` $\rightarrow$ `&`
- `&` + `&&` $\rightarrow$ `&`
- `&&` + `&` $\rightarrow$ `&`
- `&&` + `&&` $\rightarrow$ `&&`

### `std::forward` Implementation from Scratch
`std::forward<T>(arg)` performs a **conditional cast**: preserves lvalue arguments as lvalues, and rvalue arguments as rvalues.

```cpp
template <typename T>
constexpr T&& forward(std::remove_reference_t<T>& param) noexcept {
    return static_cast<T&&>(param);
}

template <typename T>
constexpr T&& forward(std::remove_reference_t<T>&& param) noexcept {
    static_assert(!std::is_lvalue_reference_v<T>, "Cannot forward rvalue as lvalue");
    return static_cast<T&&>(param);
}
```

```cpp
// Factory pattern utilizing Perfect Forwarding
template <typename T, typename... Args>
std::unique_ptr<T> make_unique_custom(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
```

---

## 5. Copy Elision, RVO & NRVO Performance Traps

### 1. Guaranteed Copy Elision (C++17)
In C++17, when a function returns a **prvalue** (temporary object), copy elision is mandatory by specification. The compiler constructs the object **directly inside the caller's stack frame allocation**, completely eliminating copy/move constructors.

```cpp
Widget createWidget() {
    return Widget(); // C++17 Guaranteed Copy Elision -> 0 copies, 0 moves!
}
Widget w = createWidget(); // Constructed in-place inside 'w'
```

### 2. Named Return Value Optimization (NRVO)
NRVO applies when returning a named local variable. It is a compiler optimization (heuristic):
```cpp
Widget createNamed() {
    Widget w;
    return w; // NRVO: Compiler constructs 'w' directly in caller's memory space
}
```

### 3. THE ANTI-PATTERN: Returning `std::move(local)`
Calling `std::move` on a local variable being returned is an performance **anti-pattern**!

```cpp
// BAD / ANTI-PATTERN:
Widget badCreate() {
    Widget w;
    return std::move(w); // DISABLES NRVO! Forces a move constructor call instead of 0-cost elision!
}

// GOOD:
Widget goodCreate() {
    Widget w;
    return w; // Enables NRVO! If NRVO fails, language automatically implicitly moves 'w'!
}
```

---

## 6. Code Scenarios & Interview Gotchas

### Scenario 1: `std::unique_ptr` Return
```cpp
std::unique_ptr<Widget> create() {
    auto w = std::make_unique<Widget>();
    return std::move(w); // PESSIMIZATION! Remove std::move for optimal NRVO!
}
```

### Scenario 2: Copy vs Move in Constructor Initializer Lists
```cpp
class Employee {
    std::string name_;
public:
    // Takes by value, moves into member variable
    Employee(std::string name) : name_(std::move(name)) {}
};
```
- **If passed rvalue (`Employee("Alice")`)**: 1 move to parameter + 1 move to `name_` = **2 moves, 0 copies**.
- **If passed lvalue (`Employee(s)`)**: 1 copy to parameter + 1 move to `name_` = **1 copy, 1 move**.
