# Const Correctness & Type System — Hard Questions

---

**Q1. What is `consteval` (C++20)?**

A: A function that **must** be evaluated at compile time — cannot be called at runtime:

```cpp
consteval int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; i++) result *= i;
    return result;
}

constexpr int f5 = factorial(5);  // OK — compile time
// int n = 5; int fn = factorial(n); // ERROR — n is not compile-time constant

// Compare:
// constexpr → CAN be compile-time, CAN be runtime
// consteval → MUST be compile-time (compile error otherwise)
```

Use `consteval` when you want to **guarantee** no runtime evaluation.

---

**Q2. What is `constinit` (C++20)?**

A: Guarantees that a variable is initialized at compile time, but it can be **used and modified at runtime**. Solves the "static initialization order fiasco."

```cpp
constinit int globalCounter = 0;      // initialized at compile time
// globalCounter++; in main() is fine — it's not const

// The problem it solves:
// Without constinit, static variables may be initialized in unpredictable order
// across translation units. constinit forces compile-time initialization.

constinit static std::atomic<int> flag{0};  // OK — constexpr constructor
```

| | `const` | `constexpr` | `consteval` | `constinit` |
|---|---|---|---|---|
| Compile-time init | Maybe | Yes | Yes | Yes |
| Runtime use | Read only | Read only | N/A (functions only) | Read/write |
| On functions | No | Yes | Yes | No |
| On variables | Yes | Yes | No | Yes (static/thread_local only) |

---

**Q3. Explain top-level vs low-level const.**

A:
- **Top-level const**: the object itself is const (ignored in type deduction)
- **Low-level const**: what the object points/refers to is const (preserved in deduction)

```cpp
const int x = 42;       // top-level const on x
const int* p = &x;      // low-level const (pointed-to is const)
int* const q = &y;      // top-level const on q (q itself is const)
const int* const r = &x; // both top-level (on r) and low-level (on *r)

// In template deduction, top-level const is stripped:
template <typename T>
void f(T val);
const int ci = 42;
f(ci);    // T = int (top-level const stripped)

template <typename T>
void g(T& val);
g(ci);    // T = const int (low-level const preserved)
```

---

**Q4. What is the `std::bit_cast` (C++20) and why was it needed?**

A: Type-safe bit reinterpretation — the correct way to do type punning:

```cpp
// BEFORE C++20 — all of these have issues:
float f = 3.14f;
int i1 = *reinterpret_cast<int*>(&f);        // UB: strict aliasing violation
int i2;  std::memcpy(&i2, &f, sizeof(int));  // OK but not constexpr

// C++20:
int i3 = std::bit_cast<int>(f);  // safe, constexpr-friendly, no UB
// Requires: sizeof(int) == sizeof(float), both trivially copyable
```

`std::bit_cast` is `constexpr`, well-defined, and doesn't violate strict aliasing.

---

**Q5. What is strict aliasing?**

A: The compiler assumes that pointers of unrelated types do not alias (point to the same memory). Violating this assumption is **undefined behavior**.

```cpp
float f = 3.14f;
int* ip = reinterpret_cast<int*>(&f);
int i = *ip;  // UB! int* and float* are unrelated — strict aliasing violation

// The compiler may optimize based on the assumption that
// modifying *ip does NOT affect f:
*ip = 0;
// Compiler might still use the old value of f — it "knows" int* can't affect float
```

Exceptions (allowed aliasing):
- `char*` and `unsigned char*` can alias anything
- `std::byte*` can alias anything (C++17)
- Related types in an inheritance hierarchy

---

## Coding Questions

---

**C1. Const-correct interface with deduplication**

Implement const and non-const `operator[]` without duplicating the logic (Scott Meyers technique):

A:
```cpp
class StringBuffer {
    std::string data_;
public:
    StringBuffer(std::string s) : data_(std::move(s)) {}

    // Const version — the real implementation
    const char& operator[](size_t i) const {
        // bounds checking, logging, etc.
        return data_[i];
    }

    // Non-const version — delegates to const version
    char& operator[](size_t i) {
        // Cast this to const, call const version, cast away const on result
        return const_cast<char&>(
            static_cast<const StringBuffer&>(*this)[i]
        );
    }
};
```

This is safe because the non-const method is only callable on non-const objects, so we know the underlying data is non-const.

---

**C2. `constexpr` compile-time map**

A:
```cpp
template <typename Key, typename Value, size_t N>
struct ConstexprMap {
    std::array<std::pair<Key, Value>, N> data;

    constexpr Value at(const Key& key) const {
        for (const auto& [k, v] : data) {
            if (k == key) return v;
        }
        throw std::logic_error("key not found");  // compile error in constexpr context
    }
};

constexpr ConstexprMap<int, const char*, 3> statusCodes = {{
    {200, "OK"},
    {404, "Not Found"},
    {500, "Internal Server Error"},
}};

constexpr auto msg = statusCodes.at(404);  // "Not Found" at compile time
// constexpr auto bad = statusCodes.at(999);  // Compile error!
```

---

**C3. Type-safe unit system with `constexpr`**

A:
```cpp
template <int M, int Kg, int S>  // meters, kilograms, seconds
struct Unit {
    double value;
    constexpr explicit Unit(double v) : value(v) {}
};

using Meters   = Unit<1, 0, 0>;
using Seconds  = Unit<0, 0, 1>;
using MPerS    = Unit<1, 0, -1>;   // meters per second
using MPerS2   = Unit<1, 0, -2>;   // meters per second squared

// Division: m / s = m/s (compile-time dimension checking)
template <int M1, int Kg1, int S1, int M2, int Kg2, int S2>
constexpr Unit<M1-M2, Kg1-Kg2, S1-S2>
operator/(Unit<M1,Kg1,S1> a, Unit<M2,Kg2,S2> b) {
    return Unit<M1-M2, Kg1-Kg2, S1-S2>(a.value / b.value);
}

constexpr Meters distance(100.0);
constexpr Seconds time(9.58);
constexpr MPerS speed = distance / time;  // 10.44 m/s — type-checked!
// constexpr Meters bad = distance / time; // COMPILE ERROR — wrong units!
```

---

**C4. Demonstrate all casts with safety commentary**

A:
```cpp
// static_cast — safe, compile-time checked
double d = 3.14;
int i = static_cast<int>(d);                    // ✅ intentional narrowing
Base* bp = static_cast<Base*>(new Derived());    // ✅ upcast (always safe)
Derived* dp = static_cast<Derived*>(bp);         // ⚠️ downcast (unchecked!)

// dynamic_cast — safe, runtime checked
Derived* dp2 = dynamic_cast<Derived*>(bp);       // ✅ nullptr if wrong type
// Needs virtual function in Base. ~100x slower than static_cast.

// const_cast — safe only if original is non-const
int x = 42;
const int& cref = x;
int& ref = const_cast<int&>(cref);               // ✅ x is non-const
const int cx = 42;
int* p = const_cast<int*>(&cx);
// *p = 100;                                     // ❌ UB — cx is truly const

// reinterpret_cast — dangerous, use sparingly
char* bytes = reinterpret_cast<char*>(&x);        // ✅ char* can alias anything
float* fp = reinterpret_cast<float*>(&x);         // ❌ strict aliasing violation
uintptr_t addr = reinterpret_cast<uintptr_t>(&x); // ✅ pointer-to-int
```
