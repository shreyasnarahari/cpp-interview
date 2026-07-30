# Theory & Mechanics: Exception Handling & noexcept

## 1. The Three Exception Safety Guarantees

```
                              Exception Safety Spectrum:
  ┌───────────────────────┬────────────────────────────────┬───────────────────────────┐
  │   Basic Guarantee     │   Strong Guarantee (Rollback)  │    No-Throw (noexcept)    │
  ├───────────────────────┼────────────────────────────────┼───────────────────────────┤
  │ Zero resource leaks;  │ Operation succeeds entirely or │ Guaranteed to never throw.│
  │ objects remain valid  │ rolls back to initial state    │ Required for destructors  │
  │ but unspecified.      │ (Copy-and-Swap Idiom).         │ & move operations.        │
  └───────────────────────┴────────────────────────────────┴───────────────────────────┘
```

### 1. Basic Guarantee
If an exception is thrown, no resources (memory, file handles, locks) are leaked, and all objects remain in a valid, uncorrupted internal state. However, values may have changed.

### 2. Strong Guarantee (Commit-or-Rollback)
If an operation fails with an exception, the state of the program is left exactly as it was prior to calling the function (as if it was never invoked).
- **Implementation via Copy-and-Swap**: Perform operations on a local copy first; if successful, swap state with `noexcept` move/swap operations.

### 3. No-Throw Guarantee (`noexcept`)
The function promises never to emit an exception. If an exception escapes a `noexcept` function, **`std::terminate()` is called immediately**, killing the process.

---

## 2. Zero-Cost Itanium ABI Exception Handling Mechanics

Modern C++ compilers implement **Zero-Cost Exception Handling** (Itanium ABI):

### 1. Happy Path ($0$ CPU Instruction Overhead)
Functions contain **zero runtime `if` checks** or exception-tracking flags. Normal code executes at maximum hardware speed without branch penalties.

### 2. Sad Path (Throw Exception Mechanics)
When `throw` occurs, the runtime performs expensive stack unwinding:
1. Allocates exception object memory in a dedicated thread-local exception buffer (`__cxa_allocate_exception`).
2. Looks up the current instruction pointer (`RIP`) in the read-only `.eh_frame` DWARF unwinding tables.
3. **Stack Unwinding**: Walks back up the stack frame by frame, invoking destructors for all stack-allocated local RAII objects in reverse order of construction.
4. Transfers control to matching catch block landing pad (`__cxa_begin_catch`).

---

## 3. `noexcept` Specifier vs `noexcept` Operator

### `noexcept` Specifier (Function Signature)
Informs the compiler that a function will not throw exceptions, enabling optimizations like register reuse, vector `move_if_noexcept`, and dead code branch elimination:

```cpp
void safe_func() noexcept; // Promises never to throw
```

### `noexcept(expr)` Operator (Compile-Time Query)
Evaluates at compile time to `true` or `false` based on whether `expr` can throw:

```cpp
template <typename T>
void swap(T& a, T& b) noexcept(noexcept(T(std::move(a)))) {
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
}
```

---

## 4. Re-throwing Exceptions: `throw;` vs `throw e;`

### 1. `throw;` (Re-throw Original Polymorphic Exception)
Re-throws the active exception object without modification or slicing. Preserves the exact original derived exception type and call stack context:

```cpp
try {
    throw DerivedException();
} catch (const BaseException& e) {
    // Correct: Re-throws DerivedException polymorphically!
    throw; 
}
```

### 2. `throw e;` (Slices and Re-throws Copy)
Constructs a **NEW exception object** by copying `e`. If caught by value or base reference, `throw e;` causes **object slicing**, losing the derived exception type!

---

## 5. Destructors and Exception Double-Fault Traps

In C++11 and later, all destructors are **implicitly `noexcept(true)`**.

### The Double-Fault Crash:
If an exception is currently unwinding the stack, and a local object's destructor throws a **second exception**:
- Two active exceptions exist simultaneously in the thread.
- The C++ runtime cannot resolve which catch block to execute.
- **`std::terminate()` is called immediately**, crashing the process without executing remaining destructors!

**CRITICAL RULE**: Destructors MUST NEVER allow exceptions to escape! Catch and handle or swallow any errors inside the destructor.

---

## 6. Code Traps & Interview Gotchas

### Code Trap 1: Throwing inside `noexcept` Function
```cpp
void f() noexcept {
    throw std::runtime_error("oops");
}
int main() {
    f(); // CRASHES IMMEDIATELY: std::terminate() called! (Catch blocks in main cannot intercept it!)
}
```

### Code Trap 2: Double Exception Termination
```cpp
struct Widget {
    ~Widget() noexcept(false) { throw std::runtime_error("dtor"); }
};
void f() {
    try {
        Widget w;
        throw std::runtime_error("body"); // Exception 1: Unwinds stack -> calls ~Widget()
        // ~Widget() throws Exception 2 -> TWO active exceptions!
    } catch (...) {
        std::cout << "caught"; // NEVER REACHED! std::terminate() terminates process!
    }
}
```

### Code Trap 3: Memory Leak on Exception in Raw Pointer Function
```cpp
void process() {
    int* data = new int[1000];
    riskyOperation(); // Might throw exception!
    delete[] data;    // LEAK! Never reached if riskyOperation throws!
}
// FIX: Use std::vector<int> or std::unique_ptr<int[]>.
```
