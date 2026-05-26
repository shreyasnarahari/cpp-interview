# Undefined Behavior & Pitfalls — Hard Questions

---

**Q1. What is strict aliasing and why does it matter?**

A: The **strict aliasing rule** says the compiler assumes that pointers of **unrelated types** do not point to the same memory. This enables aggressive optimization.

```cpp
void update(int* ip, float* fp) {
    *ip = 42;
    *fp = 3.14f;
    std::cout << *ip;  // Compiler assumes ip and fp don't alias
                        // → it may cache *ip = 42 in a register
                        // → prints 42, even if ip == (int*)fp
}
```

Violations:
```cpp
float f = 3.14f;
int i = *reinterpret_cast<int*>(&f);  // UB! strict aliasing violation
```

**Allowed aliasing** (exceptions):
- `char*`, `unsigned char*`, `std::byte*` can alias anything
- Base and derived class pointers
- A type and its signed/unsigned variant

**C++20 fix:** Use `std::bit_cast<int>(f)` for type punning — it's well-defined.

---

**Q2. What is the One Definition Rule (ODR)?**

A: Every entity (function, variable, class) must have **exactly one definition** in the entire program (across all translation units). Violations are **UB** — usually no diagnostic.

```cpp
// file1.cpp
struct Config { int x; int y; };
void f() { Config c; c.x = 1; }

// file2.cpp
struct Config { double x; };  // DIFFERENT definition — ODR violation!
void g() { Config c; c.x = 3.14; }

// Linker may silently pick one definition. Result: memory corruption.
```

How to avoid: put class definitions in header files (single definition) or use unnamed namespaces for file-local definitions.

---

**Q3. What are the consequences of UB in practice?**

A: The compiler can:

1. **Delete "dead" code** that only executes when UB occurs:
```cpp
void f(int* p) {
    int x = *p;         // compiler assumes p != null (otherwise UB)
    if (p == nullptr)    // this branch is "impossible"
        handleNull();    // compiler REMOVES this
}
```

2. **Assume loop termination:**
```cpp
while (true) {
    if (someCondition) break;
}
// Compiler can assume this terminates (infinite loops without side effects are UB in C++)
```

3. **Reorder or eliminate stores:**
```cpp
int x = computeExpensive();
*nullptr = 42;  // UB
// Compiler may move computeExpensive() AFTER the null deref
// or eliminate it entirely (since UB → anything goes)
```

---

**Q4. What is `std::launder` (C++17) and when is it needed?**

A: `std::launder` tells the compiler that a pointer points to a **different object** than originally, bypassing certain assumptions:

```cpp
struct Const { const int x; };

Const c{42};
Const* p = &c;

// Placement-new a new Const over the old one:
new (&c) Const{100};

// Without launder: compiler may use cached value of x (42)
// With launder:
Const* p2 = std::launder(&c);
std::cout << p2->x;  // guaranteed: 100
```

Needed when you construct a new object in the storage of an old one (placement new) and the type has `const` or `reference` members.

---

**Q5. What are "nasal demons"?**

A: A humorous term from the C/C++ community meaning **anything can happen** when UB occurs. The standard joke is "the compiler is allowed to make demons fly out of your nose."

The serious point: UB is not just "a crash or wrong value." It means:
- Past behavior is unreliable (the program may have been wrong all along but "worked by accident")
- The compiler may have optimized other code based on UB assumptions
- The same binary may behave differently across runs, compilers, or optimization levels
- Security vulnerabilities: buffer overflows, use-after-free → arbitrary code execution

---

## Coding Questions

---

**C1. Find and fix all 5 UB bugs**

```cpp
#include <iostream>
#include <vector>
#include <climits>

int main() {
    // Bug 1: Signed overflow
    int big = INT_MAX;
    int result = big + 1;

    // Bug 2: Out of bounds
    int arr[3] = {1, 2, 3};
    std::cout << arr[3];

    // Bug 3: Use after free
    int* p = new int(42);
    delete p;
    *p = 100;

    // Bug 4: Dangling reference
    std::string& ref = [&]() -> std::string& {
        std::string temp = "hello";
        return temp;
    }();

    // Bug 5: Non-virtual destructor
    struct Base { ~Base() {} };
    struct Derived : Base { std::vector<int> data{1,2,3}; };
    Base* bp = new Derived();
    delete bp;

    return 0;
}
```

A: Fixed version:
```cpp
#include <iostream>
#include <vector>
#include <climits>
#include <memory>

int main() {
    // Fix 1: Check before overflow
    int big = INT_MAX;
    if (big < INT_MAX) {
        int result = big + 1;
    }

    // Fix 2: Stay in bounds
    int arr[3] = {1, 2, 3};
    std::cout << arr[2];  // last valid index

    // Fix 3: Use smart pointer
    auto p = std::make_unique<int>(42);
    *p = 100;

    // Fix 4: Return by value
    std::string ref = []() {
        return std::string("hello");
    }();

    // Fix 5: Virtual destructor
    struct Base { virtual ~Base() = default; };
    struct Derived : Base { std::vector<int> data{1,2,3}; };
    std::unique_ptr<Base> bp = std::make_unique<Derived>();
    // automatically calls ~Derived() correctly

    return 0;
}
```

---

**C2. Demonstrate compiler UB exploitation**

```cpp
// Compile this with: g++ -O2 -o test test.cpp
// Then run: ./test

#include <iostream>

bool checkOverflow(int x) {
    // Programmer thinks: "If x+100 overflows, it wraps around to negative"
    return x + 100 < x;  // intending to detect overflow
}

int main() {
    std::cout << std::boolalpha;
    std::cout << checkOverflow(INT_MAX);  // "should" print true...
}

// At -O2, the compiler knows signed overflow is UB.
// It reasons: "x + 100 < x" can never be true (without overflow).
// It optimizes checkOverflow to: return false;
// Output: false (even for INT_MAX!)

// Fix: use unsigned arithmetic or compiler builtins
bool checkOverflowSafe(int x) {
    return x > INT_MAX - 100;
}
```

---

**C3. Thread-safety UB — a real production bug**

```cpp
// This code "works" in debug builds but fails in release:
class Singleton {
    static Singleton* instance_;
public:
    static Singleton* get() {
        if (!instance_) {                    // TOCTOU race
            instance_ = new Singleton();     // data race!
        }
        return instance_;
    }
};
Singleton* Singleton::instance_ = nullptr;

// Two threads call get() simultaneously:
// Thread A: checks instance_ == nullptr → true
// Thread B: checks instance_ == nullptr → true
// Thread A: creates instance
// Thread B: creates ANOTHER instance → double initialization, possible data corruption

// Fix: Meyer's Singleton (C++11 guarantees thread-safe static init)
class Singleton {
public:
    static Singleton& get() {
        static Singleton instance;  // thread-safe since C++11
        return instance;
    }
};
```

---

**C4. The complete UB gotcha reference table**

```
| Gotcha                           | UB/Trap                                          | Fix                                         |
|----------------------------------|--------------------------------------------------|---------------------------------------------|
| Signed integer overflow          | x + 1 may not wrap — compiler assumes it doesn't | Check before, use unsigned, __builtin       |
| Null pointer dereference         | Even checking after deref may be optimized out    | Check before deref, use references           |
| Out-of-bounds array access       | No bounds checking on raw arrays or operator[]    | Use .at(), or SafeArray wrapper              |
| Use after free                   | Memory may be reused — reads garbage              | Smart pointers, RAII                         |
| Dangling reference               | Reference to destroyed local                      | Return by value                              |
| Data race                        | Two threads, one write, no sync                   | std::atomic or std::mutex                    |
| Double free                      | delete same pointer twice                         | Smart pointers (unique_ptr)                  |
| Non-virtual destructor           | ~Derived not called through Base*                 | Virtual destructor on polymorphic bases      |
| Modifying string literal         | char* s = "hello"; s[0] = 'H'                    | Use char[] or std::string                    |
| Strict aliasing violation        | Casting int* to float* and dereferencing          | Use std::bit_cast or char* aliasing          |
| new[]/delete mismatch            | new[] requires delete[], not delete               | Use std::vector or unique_ptr<T[]>           |
| Static init order fiasco         | Cross-TU static init order unspecified             | Function-local statics                       |
| Shifting >= bit width            | 1 << 32 on 32-bit int                            | Check shift amount < sizeof(T)*CHAR_BIT     |
| Missing return in non-void       | Falling off end of function without return        | Enable -Wreturn-type, always return          |
```
