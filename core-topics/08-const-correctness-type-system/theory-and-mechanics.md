# Theory & Mechanics: Const Correctness & Type System

## 1. `const` Propagation — The Read-Right-to-Left Rule

Read pointer/reference declarations from **right to left**:

```cpp
const int* p;        // pointer to const int — can't modify *p, can reassign p
int const* p;        // same as above (const and int commute)
int* const p;        // const pointer to int — can modify *p, can't reassign p
const int* const p;  // const pointer to const int — can't modify either
const int& r;        // reference to const int — can't modify through r
```

### `const` member functions:
```cpp
class Widget {
    int data;
public:
    int get() const {
        // 'this' is const Widget* — all members are effectively const
        // data++;  // ERROR — can't modify members
        return data;
    }

    // Const overloading — provide both versions:
    int& at(size_t i)       { return buf[i]; }       // called on non-const objects
    const int& at(size_t i) const { return buf[i]; } // called on const objects
};
```

## 2. `constexpr` Evaluation Model

`constexpr` functions exist in a dual world — they can be evaluated at **compile time OR runtime**:

```cpp
constexpr int square(int x) { return x * x; }

// Compile-time evaluation (guaranteed):
constexpr int a = square(5);  // a = 25 at compile time
static_assert(square(5) == 25);  // compile-time check

// Runtime evaluation (allowed):
int runtime_val = get_input();
int b = square(runtime_val);  // computed at runtime — still valid

// Key: constexpr is a PERMISSION to evaluate at compile time, not a mandate
```

### `constexpr` evolution across standards:
| Standard | What's allowed in constexpr functions |
|---|---|
| C++11 | Single return statement only |
| C++14 | Loops, local variables, multiple statements |
| C++17 | `if constexpr` (compile-time branch elimination) |
| C++20 | `virtual` calls, `new`/`delete`, `try`/`catch`, `std::vector`, `std::string` |

## 3. `consteval` vs `constexpr` vs `constinit`

```
                    Evaluated at        Can run at       Variable or
                    compile time?       runtime?         function?
constexpr           May be              Yes              Both
consteval (C++20)   MUST be             No               Function only
constinit (C++20)   Init must be        Yes (after init) Variable only
```

```cpp
consteval int must_ct(int x) { return x * 2; }
constexpr int may_ct(int x) { return x * 2; }

constexpr int a = must_ct(5);    // OK — compile time
constexpr int b = may_ct(5);     // OK — compile time
// int c = must_ct(runtime_val); // ERROR — consteval requires compile-time args
int d = may_ct(runtime_val);     // OK — runtime fallback

// constinit solves static initialization order fiasco:
constinit int global = may_ct(100);  // guaranteed initialized at compile time
// global += 10;  // OK — can be modified at runtime
```

## 4. `mutable` — Logical Const vs Physical Const

`mutable` members can be modified even in `const` member functions. This supports **logical const** — the object appears unchanged to the outside world, but internal bookkeeping (caching, logging, locking) is updated:

```cpp
class LookupTable {
    mutable std::mutex mtx_;           // must lock in const methods
    mutable std::optional<int> cache_; // cache result

public:
    int expensive_lookup(int key) const {
        std::lock_guard lock(mtx_);     // OK — mtx_ is mutable
        if (cache_) return *cache_;     // OK — cache_ is mutable
        int result = /* heavy computation */;
        cache_ = result;                // OK — modifying mutable in const method
        return result;
    }
};
```

## 5. Type Casting — What the Compiler Generates

### `static_cast` — Compile-time checked, no runtime cost:
```cpp
double d = 3.14;
int i = static_cast<int>(d);
// Assembly: cvttsd2si eax, xmm0  (floating-point to integer conversion)
// Zero overhead beyond the conversion instruction itself
```

### `dynamic_cast` — Runtime type check using RTTI:
```cpp
Base* bp = get_object();
Derived* dp = dynamic_cast<Derived*>(bp);
// Runtime: inspects vtable → follows RTTI type_info chain
// Compares target type against actual object type
// Returns nullptr if types don't match (for pointers)
// Throws std::bad_cast if types don't match (for references)
// Requires at least one virtual function (needs vtable for RTTI)
// Cost: ~100-200 ns (traverses type hierarchy)
```

### `const_cast` — Zero-cost, just a type system override:
```cpp
const int* cp = &x;
int* p = const_cast<int*>(cp);
// Assembly: NOTHING — same pointer, just different type in compiler's view
// UB if original object was declared const (compiler may have placed it in .rodata)
```

### `reinterpret_cast` — Raw bit reinterpretation:
```cpp
int* ip = &x;
char* cp = reinterpret_cast<char*>(ip);
// Assembly: NOTHING — same bits, different type interpretation
// Legal for char*/unsigned char*/std::byte* (aliasing exemption)
// UB for most other type pairs (violates strict aliasing)
```
