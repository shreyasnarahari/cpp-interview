# Const Correctness & Type System — Medium Questions

---

**Q1. What is the `mutable` keyword?**

A: Allows a member to be modified even in a `const` member function. Used when the member is logically separate from the object's observable state.

```cpp
class TextDocument {
    std::string text_;
    mutable int accessCount_ = 0;      // logical const — doesn't affect "value"
    mutable std::mutex mtx_;           // must be mutable for const thread-safe methods
public:
    std::string getText() const {
        std::lock_guard lock(mtx_);    // OK — mtx_ is mutable
        accessCount_++;                 // OK — mutable member
        return text_;
    }
};
```

Real-world uses:
1. **Caching** — cache computed results without changing logical state
2. **Mutexes** — lock in const methods for thread safety
3. **Debug counters** — access tracking

---

**Q2. When is `const_cast` safe vs undefined behavior?**

A: `const_cast` is **safe** only when the original object is NOT const:

```cpp
// SAFE — original is non-const
int x = 42;
const int& ref = x;             // ref is const, but x is not
const_cast<int&>(ref) = 100;    // OK — x was non-const originally
std::cout << x;                 // 100

// UNDEFINED BEHAVIOR — original IS const
const int ci = 42;
int* p = const_cast<int*>(&ci);
*p = 100;                       // UB! ci was declared const
// Compiler may have replaced ci with literal 42 everywhere
```

**Rule:** Only use `const_cast` to interface with APIs that take non-const but promise not to modify (legacy C APIs).

---

**Q3. Explain `volatile`. Is it for multithreading?**

A: `volatile` tells the compiler: "don't optimize reads/writes to this variable — the value may change outside the program's control."

```cpp
volatile int* hardwareRegister = reinterpret_cast<volatile int*>(0xFFFF0000);
// Compiler MUST read from hardware every time — no caching in registers
```

**NOT for multithreading.** `volatile` does NOT provide:
- Atomicity (read-modify-write can be interrupted)
- Memory ordering (CPU can reorder around volatile accesses)
- Synchronization (no happens-before relationship)

For threads: use `std::atomic<T>` or `std::mutex`.

The only valid uses of `volatile`:
1. Memory-mapped I/O (hardware registers)
2. Signal handlers (with `volatile sig_atomic_t`)
3. `setjmp`/`longjmp` interactions

---

**Q4. What is the east const vs west const debate?**

A: Two styles of writing `const`:

```cpp
// West const (traditional) — const on the left
const int x = 42;
const int* p = &x;

// East const (consistent) — const always on the right
int const x = 42;
int const* p = &x;
```

East const is more consistent because `const` always modifies what's to its **left**:
```cpp
int const* const p;  // read right-to-left: const pointer to const int
// vs
const int* const p;  // same meaning, but the first const breaks the pattern
```

Both are valid. Most codebases use west const. The C++ Core Guidelines use west const.

---

**Q5. What is `reinterpret_cast`?**

A: Reinterprets the bit pattern of a value as a different type. The most dangerous cast.

```cpp
// Convert between unrelated pointer types
int x = 42;
char* bytes = reinterpret_cast<char*>(&x);
// bytes[0], bytes[1], bytes[2], bytes[3] are the raw bytes of x

// Convert pointer to integer (and back)
uintptr_t addr = reinterpret_cast<uintptr_t>(&x);
int* p = reinterpret_cast<int*>(addr);

// WARNING: strict aliasing violation
float f = 3.14f;
int i = *reinterpret_cast<int*>(&f);  // UB! violates strict aliasing
// Use std::bit_cast<int>(f) in C++20 instead
```

**Use when:** interfacing with hardware, serialization at byte level, pointer-to-integer round-trips.
**Prefer `std::bit_cast`** (C++20) for type punning.

---

## Coding Questions

---

**C1. Thread-safe cache with `mutable`**

A:
```cpp
class ExpensiveComputer {
    mutable std::mutex mtx_;
    mutable std::optional<double> cached_result_;
    std::vector<double> data_;

public:
    ExpensiveComputer(std::vector<double> data) : data_(std::move(data)) {}

    double compute() const {
        std::lock_guard lock(mtx_);
        if (cached_result_) return *cached_result_;

        // Expensive computation
        double result = 0;
        for (double d : data_) result += d * d;
        cached_result_ = result;
        return result;
    }
};

// Even though compute() is const, it can cache and lock — thanks to mutable
const ExpensiveComputer ec({1.0, 2.0, 3.0});
double r1 = ec.compute();  // computes and caches
double r2 = ec.compute();  // returns cached result
```

---

**C2. Const-correct Matrix with `operator()`**

A:
```cpp
class Matrix {
    std::vector<double> data_;
    size_t rows_, cols_;
public:
    Matrix(size_t r, size_t c) : data_(r * c, 0.0), rows_(r), cols_(c) {}

    double& operator()(size_t r, size_t c) {
        return data_[r * cols_ + c];
    }

    const double& operator()(size_t r, size_t c) const {
        return data_[r * cols_ + c];
    }

    // Avoid duplicating logic — non-const calls const version
    // (Scott Meyers' technique, Effective C++ Item 3):
    // double& operator()(size_t r, size_t c) {
    //     return const_cast<double&>(
    //         static_cast<const Matrix&>(*this)(r, c));
    // }

    size_t rows() const { return rows_; }
    size_t cols() const { return cols_; }
};

void print(const Matrix& m) {
    for (size_t r = 0; r < m.rows(); r++)
        for (size_t c = 0; c < m.cols(); c++)
            std::cout << m(r, c) << " ";  // calls const version
}
```

---

**C3. Safe downcast utility**

A:
```cpp
template <typename Derived, typename Base>
Derived* safe_cast(Base* bp) {
    static_assert(std::is_base_of_v<Base, Derived>,
        "Derived must inherit from Base");
    return dynamic_cast<Derived*>(bp);
}

template <typename Derived, typename Base>
Derived& safe_cast(Base& br) {
    return dynamic_cast<Derived&>(br);  // throws std::bad_cast on failure
}

// Usage:
Base* bp = getWidget();
if (auto* dp = safe_cast<Derived>(bp)) {
    dp->derivedMethod();
}
```

---

**C4. `constexpr` lookup table**

A:
```cpp
constexpr auto generateSquares() {
    std::array<int, 100> table{};
    for (int i = 0; i < 100; i++) {
        table[i] = i * i;
    }
    return table;
}

constexpr auto squares = generateSquares();  // computed at compile time
// squares[7] == 49, no runtime computation

// Compile-time string hash table:
constexpr uint32_t hash(const char* s) {
    uint32_t h = 0;
    while (*s) h = h * 31 + static_cast<uint32_t>(*s++);
    return h;
}

constexpr auto CMD_GET  = hash("GET");
constexpr auto CMD_POST = hash("POST");
```
