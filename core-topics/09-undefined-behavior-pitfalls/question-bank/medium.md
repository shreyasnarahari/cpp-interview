# Undefined Behavior & Pitfalls — Medium Questions

---

**Q1. Can the compiler assume UB never happens?**

A: **Yes — this is the most dangerous consequence of UB.** The compiler treats UB as an impossible path and optimizes accordingly:

```cpp
bool isPositive(int x) {
    return x + 1 > x;  // mathematically always true...
}
// Compiler: "signed overflow is UB, so x+1 > x can never be false."
// Optimized to: return true;
// For x = INT_MAX, the program is wrong — but the code had UB anyway.
```

This is called **"time-travel optimization"** — UB can affect code that runs BEFORE the UB occurs:

```cpp
void f(int* p) {
    int dead = *p;     // UB if p == nullptr — compiler assumes p != nullptr
    if (p == nullptr)  // compiler REMOVES this check (it "proved" p != nullptr)
        return;
    *p = 42;           // always executes, even when p is null
}
```

---

**Q2. What is the order of evaluation for function arguments?**

A:

**Before C++17:** The order is **unspecified**. The compiler can evaluate arguments in any order:
```cpp
int i = 0;
f(i++, i++);  // UB! Two unsequenced modifications to i
```

**C++17 fixed some cases:**
- `<<` operator: left-to-right (so `cout << i++ << i++` is OK in C++17)
- Assignment: right-hand side before left-hand side
- Function arguments: **still unspecified** order in C++17!

```cpp
// Still unspecified in C++17:
f(a(), b(), c());  // a, b, c can be called in any order
// But each call is fully sequenced before the next (no interleaving)
```

---

**Q3. What is the "static initialization order fiasco"?**

A: The order of initialization of static variables across **different translation units** is unspecified:

```cpp
// file1.cpp
extern int x;
int y = x + 1;  // may execute before x is initialized → UB

// file2.cpp
int x = 42;
```

`y` might be 1 (if `x` hasn't been initialized yet, it's zero-initialized to 0) or 43 (if `x` initialized first).

**Fix:** Use a function-local static (Construct On First Use idiom):
```cpp
int& getX() {
    static int x = 42;  // guaranteed initialized on first call
    return x;
}
int& getY() {
    static int y = getX() + 1;  // always sees x = 42
    return y;
}
```

---

**Q4. Is it UB to modify a string literal?**

A: **Yes.** String literals have static storage duration and may be stored in read-only memory:

```cpp
char* s = "hello";   // s points to read-only string literal
s[0] = 'H';          // UB! May segfault, or silently corrupt memory

// Fix: use a char array (copies the string)
char s[] = "hello";  // s is a mutable array, NOT a pointer to literal
s[0] = 'H';          // OK

// Or use std::string
std::string s = "hello";
s[0] = 'H';          // OK
```

Note: `const char*` is the correct type for string literals. The implicit conversion from string literal to `char*` was deprecated in C++11 and removed in C++17.

---

**Q5. What is the "Most Vexing Parse"?**

A: An ambiguity where a declaration that looks like an object definition is actually a function declaration:

```cpp
Widget w();   // NOT an object — this is a FUNCTION declaration!
              // w is a function taking no args and returning Widget

// Fixes:
Widget w;          // default construction
Widget w{};        // brace initialization (C++11)
Widget w = Widget();  // explicit temporary + copy (usually elided)

// Also:
Widget w(Timer());     // FUNCTION: w takes a Timer(*)() and returns Widget
Widget w{Timer{}};     // OBJECT: w constructed with a Timer (unambiguous)
```

---

## Coding Questions

---

**C1. Fix the static initialization order fiasco**

```cpp
// logger.cpp
Logger& getLogger() {
    static Logger instance("app.log");
    return instance;
}

// config.cpp
Config& getConfig() {
    static Config instance("config.yaml");
    return instance;
}

// Order doesn't matter — each is initialized on first use
void init() {
    getLogger().log("Config: " + getConfig().get("name"));
}
```

---

**C2. Safe array wrapper**

A:
```cpp
template <typename T, size_t N>
class SafeArray {
    std::array<T, N> data_{};
public:
    T& operator[](size_t i) {
        if (i >= N) throw std::out_of_range(
            "Index " + std::to_string(i) + " out of range [0, " + std::to_string(N) + ")");
        return data_[i];
    }

    const T& operator[](size_t i) const {
        if (i >= N) throw std::out_of_range(
            "Index " + std::to_string(i) + " out of range [0, " + std::to_string(N) + ")");
        return data_[i];
    }

    constexpr size_t size() const { return N; }
};

SafeArray<int, 5> arr;
arr[0] = 42;     // OK
// arr[5] = 42;  // throws std::out_of_range
```

---

**C3. Demonstrate UBSan detecting real bugs**

```cpp
// Compile with: g++ -fsanitize=undefined -g -o test test.cpp

int main() {
    // 1. Signed overflow
    int x = INT_MAX;
    x++;  // UBSan: runtime error: signed integer overflow

    // 2. Null dereference
    int* p = nullptr;
    *p = 42;  // UBSan: runtime error: null pointer passed as argument

    // 3. Shift overflow
    int y = 1;
    y = y << 33;  // UBSan: runtime error: shift exponent 33 is too large

    // 4. Division by zero
    int z = 42 / 0;  // UBSan: runtime error: division by zero
}
```

---

**C4. The `delete`-through-base-without-virtual-destructor bug**

```cpp
// BUG:
struct Base {
    ~Base() { /* not virtual */ }
};
struct Derived : Base {
    std::vector<int> data{1, 2, 3, 4, 5};
};

Base* p = new Derived();
delete p;  // UB: ~Derived() never called → data leaks

// FIX:
struct Base {
    virtual ~Base() = default;
};
```

This is one of the most common real-world UB bugs in production C++ code.
