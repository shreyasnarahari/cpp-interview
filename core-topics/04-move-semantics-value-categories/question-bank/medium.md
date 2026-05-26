# Move Semantics — Medium Questions

---

**Q1. What are forwarding references?**

A: `T&&` where `T` is a deduced template parameter. NOT the same as an rvalue reference:

```cpp
template <typename T>
void f(T&& arg);  // forwarding reference — binds to BOTH lvalues and rvalues

void g(int&& arg);  // rvalue reference — only rvalues
```

Forwarding references are deduced as `T&` for lvalues and `T` for rvalues (reference collapsing).

---

**Q2. What are reference collapsing rules?**

A:
| | `&` | `&&` |
|---|---|---|
| `T&` | `T&` | `T&` |
| `T&&` | `T&` | `T&&` |

Lvalue reference always wins. Only `&& + &&` = `&&`.

```cpp
template <typename T>
void f(T&& x);

int a = 5;
f(a);    // T = int&,  T&& = int& && = int&  (lvalue ref)
f(42);   // T = int,   T&& = int&&           (rvalue ref)
```

---

**Q3. What is perfect forwarding?**

A: Preserving the value category (lvalue/rvalue) of arguments when passing them through:

```cpp
template <typename T>
void wrapper(T&& arg) {
    target(std::forward<T>(arg));  // forwards as lvalue or rvalue
}
```

`std::forward<T>(arg)` casts to `T&&`:
- If `T = int&`: `forward<int&>(arg)` → lvalue reference → passed as lvalue
- If `T = int`: `forward<int>(arg)` → rvalue reference → passed as rvalue

---

**Q4. When does guaranteed copy elision apply (C++17)?**

A: When a **prvalue** is used to initialize an object of the same type:

```cpp
Widget w = Widget(42);       // guaranteed — no copy/move at all
Widget w = createWidget();   // guaranteed if createWidget returns a prvalue
```

**Not guaranteed** for named variables (NRVO):
```cpp
Widget createWidget() {
    Widget w(42);
    return w;  // NRVO — common but NOT guaranteed
}
```

---

## Coding Questions

---

**C1. Implement `std::forward`**

A:
```cpp
template <typename T>
T&& forward(std::remove_reference_t<T>& arg) noexcept {
    return static_cast<T&&>(arg);
}

template <typename T>
T&& forward(std::remove_reference_t<T>&& arg) noexcept {
    static_assert(!std::is_lvalue_reference_v<T>,
        "Cannot forward an rvalue as an lvalue");
    return static_cast<T&&>(arg);
}
```

---

**C2. Emplace-style factory**

```cpp
template <typename T, typename... Args>
T create(Args&&... args) {
    return T(std::forward<Args>(args)...);
}

auto s = create<std::string>(5, 'x');  // "xxxxx"
auto v = create<std::vector<int>>(std::initializer_list<int>{1,2,3});
```

---

**C3. `std::move` vs `std::forward` — explain the difference**

A: `std::move` = unconditional cast to rvalue. `std::forward` = conditional cast (preserves category).

```cpp
template <typename T>
void relay(T&& arg) {
    // std::move(arg)    → ALWAYS casts to rvalue (may steal from lvalue!)
    // std::forward<T>(arg) → preserves: lvalue→lvalue, rvalue→rvalue
    target(std::forward<T>(arg));  // correct
}
```

**Rule:** Use `std::move` on rvalue references, `std::forward` on forwarding references.

---

**C4. Sink pattern (move into function)**

```cpp
class Database {
    std::string connStr_;
public:
    // Sink parameter — takes ownership
    void setConnection(std::string cs) {  // by VALUE
        connStr_ = std::move(cs);
    }
};

Database db;
std::string s = "host=localhost";
db.setConnection(s);             // copy into param, move into member
db.setConnection(std::move(s));  // move into param, move into member
db.setConnection("host=local");  // construct in param, move into member
```

Taking by value + move is the modern "sink" pattern — one function handles all cases.
