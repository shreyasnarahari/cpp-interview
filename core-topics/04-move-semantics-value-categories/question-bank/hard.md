# Move Semantics — Hard Questions

---

**Q1. What are all five value categories in C++?**

A:
```
        expression
       /          \
    glvalue      rvalue
    /    \       /    \
 lvalue  xvalue     prvalue
```

- **lvalue**: has identity, can't be moved from implicitly (named variables, `*ptr`)
- **prvalue**: no identity, movable (temporaries, `42`, `Widget()`)
- **xvalue**: has identity AND is movable (`std::move(x)`, cast to `T&&`)
- **glvalue**: lvalue | xvalue (has identity)
- **rvalue**: prvalue | xvalue (can be moved from)

```cpp
int x = 42;
// x is lvalue
// 42 is prvalue
// std::move(x) is xvalue
// x + 1 is prvalue
```

---

**Q2. Explain the interaction between move semantics and exception safety in containers.**

A: `std::vector` must provide the strong exception guarantee during reallocation. If a move constructor can throw:

1. Vector starts moving elements to new buffer
2. Element N's move throws
3. Elements 0..N-1 are already moved — original buffer is corrupted
4. Cannot rollback — source objects are in moved-from state

**Solution:** `vector` only uses move if the move constructor is `noexcept`. Otherwise it copies (which can be rolled back by destroying the copies).

```cpp
struct Safe {
    Safe(Safe&&) noexcept;  // vector will MOVE during reallocation
};
struct Unsafe {
    Unsafe(Unsafe&&);       // vector will COPY during reallocation
};
```

This is why `noexcept` on move operations is critical for performance.

---

**Q3. What is the return value optimization mechanism?**

A: The compiler allocates the return value in the **caller's stack frame** and passes a hidden pointer to the callee. The callee constructs directly into that address.

```cpp
// What the compiler roughly does:
Widget createWidget() {
    Widget w;
    return w;
}
Widget result = createWidget();

// Transformed to:
void createWidget(Widget* __result) {
    new (__result) Widget();  // construct in caller's space
}
Widget result;  // uninitialized
createWidget(&result);
```

C++17 guaranteed copy elision goes further: for prvalue returns, the temporary **never exists** as a separate object.

---

## Coding Questions

---

**C1. Implement `std::move`**

A:
```cpp
template <typename T>
constexpr std::remove_reference_t<T>&& move(T&& t) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(t);
}
```

That's it — `std::move` is literally just a cast. It removes references and casts to `T&&`.

---

**C2. Demonstrate noexcept impact on vector**

A:
```cpp
#include <iostream>
#include <vector>

struct Tracked {
    int id;
    Tracked(int id) : id(id) {}
    Tracked(const Tracked& o) : id(o.id) { std::cout << "copy " << id << "\n"; }
    Tracked(Tracked&& o) noexcept : id(o.id) { std::cout << "move " << id << "\n"; }
};

struct TrackedNothrow {
    int id;
    TrackedNothrow(int id) : id(id) {}
    TrackedNothrow(const TrackedNothrow& o) : id(o.id) { std::cout << "copy " << id << "\n"; }
    TrackedNothrow(TrackedNothrow&& o) /* no noexcept */ : id(o.id) { std::cout << "move " << id << "\n"; }
};

// With noexcept: vector uses move during reallocation → "move 0"
// Without noexcept: vector uses copy during reallocation → "copy 0"
```

---

**C3. Perfect forwarding wrapper**

Write an `invoke` wrapper that perfectly forwards arguments to any callable:

A:
```cpp
template <typename F, typename... Args>
decltype(auto) invoke(F&& f, Args&&... args) {
    return std::forward<F>(f)(std::forward<Args>(args)...);
}

int add(int a, int b) { return a + b; }
auto result = invoke(add, 3, 4);  // 7

auto lambda = [](std::string s) { return s.size(); };
auto len = invoke(lambda, std::string("hello"));  // 5
```

`decltype(auto)` preserves the exact return type (including references).

---

**C4. Move-aware linked list insert**

```cpp
template <typename T>
class List {
    struct Node {
        T data;
        std::unique_ptr<Node> next;
        template <typename U>
        Node(U&& val, std::unique_ptr<Node> next)
            : data(std::forward<U>(val)), next(std::move(next)) {}
    };
    std::unique_ptr<Node> head_;
public:
    template <typename U>
    void push_front(U&& val) {
        head_ = std::make_unique<Node>(std::forward<U>(val), std::move(head_));
    }
};

List<std::string> list;
std::string s = "hello";
list.push_front(s);             // copies s into node
list.push_front(std::move(s));  // moves s into node
list.push_front("world");       // constructs string in-place
```
