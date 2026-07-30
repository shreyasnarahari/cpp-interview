# Theory & Mechanics: Undefined Behavior & Common Pitfalls

## 1. The Behavior Spectrum: UB vs Implementation-Defined vs Unspecified

```
  Behavior Spectrum:
  ┌────────────────────────┬────────────────────────────────┬───────────────────────────┐
  │ Implementation-Defined │      Unspecified Behavior      │    Undefined Behavior     │
  ├────────────────────────┼────────────────────────────────┼───────────────────────────┤
  │ Compiler MUST document │ Valid options, but compiler    │ Zero constraints.         │
  │ choices (e.g. sizeof   │ choice is un-documented (e.g.  │ Compiler assumes UB never │
  │ int, char signedness). │ allocation memory address).    │ occurs -> Optimizes code! │
  └────────────────────────┴────────────────────────────────┴───────────────────────────┘
```

### 1. Undefined Behavior (UB)
The C++ ISO Standard places **zero constraints** on the program execution. Once UB is triggered, the program can crash, corrupt memory, format the drive, or silently emit invalid calculations.
- **Compiler Optimizations based on UB**: Compilers assume UB *never happens*. If a code path triggers UB under certain conditions, the compiler may optimize away entire `if` branches or dead code blocks!

### 2. Implementation-Defined Behavior
The behavior is valid and predictable, but varies by platform/compiler. **The compiler vendor is required to document the choice**:
- Size of `int`, `long`, `pointer` types (`sizeof(int)` is 4 on x86, 2 on 16-bit AVR).
- Whether `char` is `signed` or `unsigned`.
- Result of right-shifting a negative signed integer.

### 3. Unspecified Behavior
The standard allows multiple valid behaviors, but does **not require documentation** of which option is selected:
- Address of allocated heap memory blocks.
- Order of function argument evaluation prior to C++17 (`f(g(), h())`).

---

## 2. Top 8 Sources of Undefined Behavior

### 1. Signed Integer Overflow vs Unsigned Wrapping
- **Signed Integer Overflow IS UNDEFINED BEHAVIOR**:
  ```cpp
  int x = INT_MAX;
  x = x + 1; // UNDEFINED BEHAVIOR! Compiler may optimize away 'if (x + 1 < x)' checks!
  ```
- **Unsigned Integer Overflow IS WELL-DEFINED**: Unsigned integers are modulo arithmetic operations ($2^N$). `unsigned int u = 0; u = u - 1;` safely wraps to `UINT_MAX`.

#### Safe Signed Addition Overflow Check (Pre-execution check):
```cpp
bool safe_add(int a, int b, int& result) {
    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        return false; // Overflow would occur!
    }
    result = a + b;
    return true;
}
```

### 2. Strict Aliasing Violation
The compiler assumes pointers of different types (e.g., `int*` and `float*`) do **not point to the same memory location** (Type-Based Alias Analysis / `tbaa`).

```cpp
float f = 3.14f;
int* ip = (int*)&f; // BUG: Strict aliasing violation!
*ip = 42; // UB: Compiler assumes 'f' is unmodified and reads stale register!
```
- **Correct Fix**: Use `std::memcpy` or `std::bit_cast<int>(f)` (C++20).

### 3. Modifying String Literals
String literals (`char* s = "hello";`) are stored in the executable's **Read-Only Data Segment (`.rodata`)**.
```cpp
char* s = "hello";
s[0] = 'H'; // UNDEFINED BEHAVIOR (Segmentation Fault / OS Page Write Protection violation!)
```

### 4. Dangling Pointers & References
Returning pointers/references to stack-allocated local variables:
```cpp
int* dangling() {
    int x = 42;
    return &x; // UB: Returning address of stack variable that dies at scope exit!
}
```

### 5. Array Out of Bounds: `operator[]` vs `at()`
- `std::vector::operator[]` / raw `arr[i]`: Performs **NO bounds checking** for performance. Accessing `v[100]` when `size() == 5` is UB!
- `std::vector::at(i)`: Performs runtime bounds check. Throws `std::out_of_range` if index is out of bounds (Safe!).

### 6. Order of Evaluation Traps
Prior to C++17, sequence points for expressions like `i = i++ + ++i` or `f(i++, i++)` were un-sequenced.
```cpp
int i = 0;
std::cout << i++ << " " << i++ << " " << i++; // UB / Unspecified prior to C++17!
```

### 7. Non-Virtual Base Class Destructor Deletion
Deleting a derived object through a base pointer (`Base* p = new Derived(); delete p;`) when `Base` lacks a `virtual` destructor is UB!

### 8. Reading Uninitialized Memory
Reading uninitialized non-static local variables (`int x; std::cout << x;`) loads indeterminate garbage bits or triggers CPU trap representations.

---

## 3. The Static Initialization Order Fiasco & Fix

When global variables in separate translation units (`A.cpp` and `B.cpp`) depend on each other during static initialization, their initialization order is **undefined**:

```cpp
// A.cpp
extern int b_val;
int a_val = b_val + 1; // Might read 'b_val' BEFORE B.cpp initializes it!

// B.cpp
int b_val = 42;
```

### The Solution: Meyer's Singleton (Function-Local Static)
Function-local static variables are guaranteed to be initialized on **first function call** in a thread-safe manner (C++11):

```cpp
int& get_b_val() {
    static int b_val = 42; // Thread-safe 1st-call initialization!
    return b_val;
}

int get_a_val() {
    return get_b_val() + 1; // Guaranteed safe initialization order!
}
```

---

## 4. Runtime Sanitizers for Detecting UB

| Sanitizer Flag | Detected Pitfalls & Faults |
|---|---|
| `-fsanitize=address` (ASan) | Out-of-bounds array access, Use-After-Free, Double Free, Memory Leaks |
| `-fsanitize=undefined` (UBSan) | Signed integer overflow, null pointer dereference, misaligned pointers, shift out-of-bounds |
| `-fsanitize=thread` (TSan) | Concurrent Data Races across threads |
