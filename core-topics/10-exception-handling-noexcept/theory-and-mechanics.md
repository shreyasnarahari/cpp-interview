# Exception Handling & `noexcept` — Deep Theory & Mechanics

An exhaustive, low-level systems guide to C++ exception handling, stack unwinding, Zero-Cost Exception Handling (Itanium ABI `.eh_frame`), the `noexcept` operator and specifier, exception safety guarantees, and constructor function-try-blocks.

---

## 1. Exception Handling Architecture & Stack Unwinding

When an exception is thrown in C++, execution halts immediately, and the runtime begins **Stack Unwinding**:

```
[ Function A (try { Function B(); } catch(...)) ]
       ^
       | 3. Searches call frames for matching catch block (Personality Routine)
       |
[ Function B (Local Objects D1, D2) ]
       ^
       | 2. Stack Unwinding: Calls ~D2(), then ~D1() in REVERSE construction order
       |
[ Function C (Local Object D3) ]
       ^
       | 1. `throw std::runtime_error("error")`
```

### 1.1 Fatal Rule: Why Destructors Must NEVER Throw!
If an exception is already in flight (during stack unwinding) and a local object's destructor throws a **second unhandled exception**, the C++ runtime immediately invokes **`std::terminate()`**, aborting the process!
- Since C++11, all destructors are **implicitly `noexcept(true)`**.
- If a destructor must execute operations that might fail, it **must catch and handle all exceptions internally**!

---

## 2. Zero-Cost Exception Handling (Itanium C++ ABI)

Modern 64-bit C++ compilers implement **Table-Based "Zero-Cost" Exception Handling**.

```
+-------------------------------------------------------------------------------+
| HAPPY PATH (No Exceptions Thrown): ZERO RUNTIME OVERHEAD!                     |
|                                                                               |
|  - Normal function calls and returns execute without ANY try/catch checks.    |
|  - Zero CPU instructions generated for entering or exiting a `try` block.     |
+-------------------------------------------------------------------------------+
                                        | (Exception is thrown!)
                                        v
+-------------------------------------------------------------------------------+
| EXCEPTIONAL PATH (High Overhead):                                             |
|                                                                               |
|  1. Runtime calls `_Unwind_RaiseException()`.                                 |
|  2. Binary searches `.eh_frame` and `.gcc_except_table` static tables.        |
|  3. Executes DWARF unwind instructions to restore registers & stack frames.   |
|  4. Calls Personality Routine to find matching Landing Pad catch handler.     |
+-------------------------------------------------------------------------------+
```

### 2.1 The Tradeoff of Zero-Cost Exceptions:
- **Happy Path**: **Zero overhead** (0 CPU cycles, pure sequential execution).
- **Exceptional Path**: **Very expensive** ($1\text{ }\mu\text{s} - 10\text{ }\mu\text{s}$ per throw due to table searches and personality routine execution).
- **Code Size**: The binary includes `.eh_frame` metadata tables (~15–20% binary size increase).

---

## 3. The `noexcept` Specifier & Operator

### 3.1 `noexcept` Specifier
Declares whether a function can throw exceptions:
- `void f() noexcept;` (Equivalent to `noexcept(true)`)
- `void g() noexcept(false);`
- If an exception escapes a `noexcept` function, `std::terminate()` is called immediately without guaranteeing complete stack unwinding.

### 3.2 `noexcept` Operator
Evaluates at compile-time whether an expression is declared not to throw:

```cpp
template <typename T>
void swap(T& a, T& b) noexcept(noexcept(T(std::move(a))) && noexcept(a = std::move(b))) {
    T temp = std::move(a);
    a = std::move(b);
    b = std::move(temp);
}
```

---

## 4. The 4 Exception Safety Levels

| Guarantee Level | State Invariant on Exception | Code Construction Idiom |
|---|---|---|
| **1. Nothrow (No-fail)** | The operation is guaranteed never to throw (`noexcept`). | Destructors, move constructors, `swap()`, deallocation routines. |
| **2. Strong Guarantee** | **Commit-or-Rollback**: The program state is unaffected (reverts to state before call). | Copy-and-Swap idiom, functional immutable transformations. |
| **3. Basic Guarantee** | **No leaks, valid invariants**: No memory/handles leaked; objects remain in consistent, destructible states. | RAII wrappers for all resources. |
| **4. No Guarantee** | Undefined behavior, leaked resources, corrupted data structures. | Raw pointer manipulation, non-RAII manual cleanup. |

---

## 5. Function-Try-Blocks in Constructors

Used to catch exceptions thrown by member initializer lists or base class constructors:

```cpp
class DatabaseSession {
    Connection conn_;
    Logger log_;
public:
    // Function-Try-Block
    DatabaseSession(std::string url) try 
        : conn_(url), log_("session.log") {
        // Constructor body
    } catch (const std::exception& e) {
        // Log initialization failure
        std::cerr << "Init failed: " << e.what() << "\n";
        // CRITICAL: In a constructor catch block, C++ AUTOMATICALLY RE-THROWS
        // the exception at the end of the block! You CANNOT suppress it!
    }
};
```
