# Move Semantics — Easy Questions

---

**Q1. What is an lvalue vs an rvalue?**

A: An **lvalue** has identity (you can take its address). An **rvalue** is a temporary with no persistent identity.

```cpp
int x = 42;       // x is an lvalue
int* p = &x;      // OK — lvalues are addressable

int y = x + 1;    // (x + 1) is an rvalue — temporary result
// int* q = &(x+1); // ERROR — can't take address of rvalue

std::string s = "hello";        // s is lvalue, "hello" is rvalue
std::string t = s + " world";   // (s + " world") is rvalue
```

---

**Q2. What does `std::move` do?**

A: `std::move` is just an **unconditional cast to `T&&`** (rvalue reference). It does NOT move anything. It signals that an object may be moved from.

```cpp
std::string a = "hello";
std::string b = std::move(a);  // std::move casts a to string&&
                                // string's move constructor actually moves
// a is now in a valid-but-unspecified state (likely empty)
```

`std::move` is essentially: `static_cast<std::remove_reference_t<T>&&>(t)`

---

**Q3. What is a move constructor?**

A: A constructor that "steals" resources from a temporary instead of copying:

```cpp
class Buffer {
    int* data_;
    size_t size_;
public:
    // Move constructor — steal from source
    Buffer(Buffer&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;  // source is now empty
        other.size_ = 0;
    }
};
```

Move is cheap (pointer swap) vs copy (deep copy all data). Called for rvalues/temporaries.

---

**Q4. What is the moved-from state?**

A: **Valid but unspecified.** A moved-from object must still be destructible and assignable, but its value is unspecified.

```cpp
std::string s = "hello";
std::string t = std::move(s);
// s is valid — you can assign to it, destroy it, call .size()
// But s.size() might be 0, or something else — don't rely on it
s = "new value";  // perfectly fine — reassigning
```

---

**Q5. What is copy elision / RVO?**

A: The compiler can skip copy/move operations and construct the object directly at the call site:

```cpp
Widget createWidget() {
    return Widget(42);  // RVO — constructed directly in caller's space
}
Widget w = createWidget();  // No copy, no move — just one construction
```

- **RVO**: Return Value Optimization — for returning temporaries (prvalues). **Mandatory in C++17.**
- **NRVO**: Named RVO — for returning named local variables. Not guaranteed but common.

---

## Coding Questions

---

**C1. Move-only Buffer class**

A:
```cpp
class Buffer {
    std::unique_ptr<int[]> data_;
    size_t size_;
public:
    explicit Buffer(size_t n) : data_(std::make_unique<int[]>(n)), size_(n) {}

    // Move constructor
    Buffer(Buffer&& other) noexcept = default;
    // Move assignment
    Buffer& operator=(Buffer&& other) noexcept = default;

    // Delete copy
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    int* data() { return data_.get(); }
    size_t size() const { return size_; }
};
```

---

**C2. Predict the output**

```cpp
struct Widget {
    Widget() { std::cout << "default "; }
    Widget(const Widget&) { std::cout << "copy "; }
    Widget(Widget&&) noexcept { std::cout << "move "; }
    Widget& operator=(const Widget&) { std::cout << "copy= "; return *this; }
    Widget& operator=(Widget&&) noexcept { std::cout << "move= "; return *this; }
};

Widget a;           // default
Widget b = a;       // copy
Widget c = std::move(a);  // move
b = c;              // copy=
b = std::move(c);   // move=
```

A: `default copy move copy= move=`

---

**C3. Why noexcept matters**

```cpp
struct Widget {
    Widget(Widget&&) { /* no noexcept */ }
};
std::vector<Widget> v;
v.reserve(2);
v.emplace_back();
v.emplace_back();
v.emplace_back();  // vector grows — copies or moves elements?
```

A: **Copies!** When `vector` reallocates, it uses `move` only if the move constructor is `noexcept`. Without `noexcept`, it falls back to copy for strong exception safety. Fix: `Widget(Widget&&) noexcept { ... }`

---

**C4. Fix the anti-pattern**

```cpp
std::string createGreeting(const std::string& name) {
    std::string result = "Hello, " + name + "!";
    return std::move(result);  // ?
}
```

A: Remove `std::move` — it **prevents NRVO**. Without `std::move`, the compiler can construct `result` directly in the caller's space. With `std::move`, it forces a move construction. Just `return result;` is optimal.
