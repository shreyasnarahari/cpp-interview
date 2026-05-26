# Const Correctness & Type System — Easy Questions

Try to answer each question before reading the answer.

---

**Q1. What is the difference between `const int*`, `int* const`, and `const int* const`?**

A: Read **right to left**:

```cpp
const int* p1;        // pointer to const int — can't modify *p1, can reassign p1
int* const p2;        // const pointer to int — can modify *p2, can't reassign p2
const int* const p3;  // const pointer to const int — can't modify or reassign

int x = 10, y = 20;
const int* p = &x;
// *p = 5;    // ERROR — can't modify through pointer-to-const
p = &y;       // OK — pointer itself can change

int* const q = &x;
*q = 5;       // OK — can modify through the pointer
// q = &y;    // ERROR — pointer itself is const
```

Mnemonic: **`const` binds to the thing on its left** (unless it's leftmost, then it binds right).

---

**Q2. What is a const member function?**

A: A member function that promises NOT to modify the object (`*this` is const inside it):

```cpp
class Widget {
    int value_ = 42;
public:
    int getValue() const { return value_; }   // const — can be called on const objects
    void setValue(int v) { value_ = v; }      // non-const — cannot be called on const objects
};

const Widget cw;
cw.getValue();   // OK
// cw.setValue(5); // ERROR — can't call non-const method on const object

Widget w;
w.getValue();    // OK — non-const can call const methods too
w.setValue(5);   // OK
```

**Rule:** Mark every method `const` unless it modifies the object.

---

**Q3. What is the difference between `const` and `constexpr`?**

A:

| | `const` | `constexpr` |
|---|---|---|
| Meaning | "I won't modify this" | "This can be evaluated at compile time" |
| When evaluated | Runtime or compile time | Compile time (if possible) |
| On functions | N/A for free functions | Function can be called at compile time |
| On variables | Immutable | Immutable AND known at compile time |

```cpp
const int a = 42;          // immutable, MAY be compile-time constant
constexpr int b = 42;      // immutable, GUARANTEED compile-time constant

const int c = getInput();  // OK — runtime const
// constexpr int d = getInput(); // ERROR — not compile-time evaluable

constexpr int square(int x) { return x * x; }
constexpr int s = square(5);   // computed at compile time = 25
int y = 5;
int s2 = square(y);            // computed at runtime (y is not constexpr)
```

---

**Q4. What is `static_cast`?**

A: The safe, compile-time checked cast for "natural" conversions:

```cpp
// Numeric conversions
double d = 3.14;
int i = static_cast<int>(d);  // 3 — explicit narrowing

// Up/downcasting in class hierarchies (no runtime check!)
Base* bp = new Derived();
Derived* dp = static_cast<Derived*>(bp);  // OK — we know it's Derived

// void* conversions
void* vp = &i;
int* ip = static_cast<int*>(vp);
```

**Use for:** numeric conversions, upcasts, downcasts when you're sure of the type, `void*` round-trips.
**Don't use for:** unrelated types, removing const (use `const_cast`).

---

**Q5. What is `dynamic_cast`?**

A: A runtime-checked downcast for polymorphic types. Requires at least one virtual function.

```cpp
class Base { public: virtual ~Base() {} };
class Derived : public Base { public: int data = 42; };

Base* bp = new Base();
Derived* dp = dynamic_cast<Derived*>(bp);
// dp == nullptr — bp doesn't actually point to a Derived

Base* bp2 = new Derived();
Derived* dp2 = dynamic_cast<Derived*>(bp2);
// dp2 != nullptr — bp2 actually points to a Derived

// With references — throws std::bad_cast on failure:
try {
    Derived& dr = dynamic_cast<Derived&>(*bp);
} catch (const std::bad_cast& e) {
    // bp is not actually a Derived
}
```

**Overhead:** ~100x slower than `static_cast` (RTTI lookup). Use only when the type is genuinely unknown.

---

## Coding Questions

---

**C1. Const-correct `Vector` class**

A:
```cpp
class Vector {
    double* data_;
    size_t size_;
public:
    Vector(size_t n) : data_(new double[n]()), size_(n) {}
    ~Vector() { delete[] data_; }

    // Non-const version — returns mutable reference
    double& operator[](size_t i) { return data_[i]; }

    // Const version — returns const reference
    const double& operator[](size_t i) const { return data_[i]; }

    size_t size() const { return size_; }  // const — doesn't modify
};

void printFirst(const Vector& v) {
    std::cout << v[0];    // calls const operator[]
    // v[0] = 5.0;        // ERROR — can't modify through const ref
}

Vector v(10);
v[0] = 3.14;              // calls non-const operator[]
printFirst(v);
```

---

**C2. Cast each type correctly**

A:
```cpp
// 1. Numeric conversion → static_cast
double pi = 3.14159;
int truncated = static_cast<int>(pi);  // 3

// 2. Polymorphic downcast → dynamic_cast
Base* bp = getWidget();
if (auto* dp = dynamic_cast<Derived*>(bp)) {
    dp->derivedMethod();
}

// 3. Remove const (carefully) → const_cast
void legacyApi(char* s);  // old API, won't modify s
const char* msg = "hello";
legacyApi(const_cast<char*>(msg));  // OK only if legacyApi doesn't actually write

// 4. Bit reinterpretation → reinterpret_cast
uint32_t raw = 0x40490FDB;
float f = *reinterpret_cast<float*>(&raw);  // ~3.14159 (IEEE 754)
// Warning: this violates strict aliasing — use std::bit_cast (C++20) instead
```

---

**C3. Const propagation through return types**

```cpp
class Config {
    std::map<std::string, std::string> data_;
public:
    // Non-const: returns modifiable reference
    std::string& get(const std::string& key) {
        return data_[key];
    }

    // Const: returns const reference, uses at() to avoid insertion
    const std::string& get(const std::string& key) const {
        return data_.at(key);  // throws if not found (no insertion)
    }
};
```

Note: the non-const version uses `operator[]` (which inserts default on missing key), while the const version uses `at()` (which throws). This is because `operator[]` is non-const on `std::map`.

---

**C4. Demonstrate `constexpr` function**

A:
```cpp
constexpr int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int tmp = a + b;
        a = b;
        b = tmp;
    }
    return b;
}

// Compile-time:
constexpr int fib10 = fibonacci(10);  // 55 — computed at compile time
static_assert(fib10 == 55);

// Runtime:
int n;
std::cin >> n;
int result = fibonacci(n);  // computed at runtime
```
