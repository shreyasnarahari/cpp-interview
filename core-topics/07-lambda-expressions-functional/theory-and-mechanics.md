# Lambda Expressions & Functional Programming — Deep Theory & Mechanics

An exhaustive, low-level guide to C++ lambda closure class generation, capture mechanics, move/init-captures, stateless lambda function pointer decay, generic & template lambdas, `std::function` type erasure and Small Buffer Optimization (SBO), and modern C++23 functional additions (`std::move_only_function`, deducing `this`).

---

## 1. Compiler Translation of Lambda Expressions: The Closure Class

In C++, a lambda expression is **pure syntactic sugar** for generating an anonymous, unique, compiler-generated class called a **Closure Type** (Functor):

```cpp
int factor = 5;
auto lambda = [factor](int x) -> int { return x * factor; };
```

### 1.1 What the Compiler Generates Under the Hood:

```cpp
// Compiler-generated anonymous closure class:
class __lambda_unique_id {
private:
    int factor; // Captured variables become member variables!
public:
    __lambda_unique_id(int f) : factor(f) {}

    // operator() is const by default!
    int operator()(int x) const {
        return x * factor;
    }
};

// Instantiation:
__lambda_unique_id lambda(factor);
```

---

## 2. Capture Modes & Mechanics

| Capture Syntax | Semantic Meaning | Closure Class Member Representation |
|---|---|---|
| `[]` | No captures. | Empty class (`sizeof == 1` byte). |
| `[x]` | Capture `x` by copy/value. | Copy-constructed member variable `T x;`. |
| `[&x]` | Capture `x` by reference. | Reference member variable `T& x;`. |
| `[=]` | Capture all used local variables by copy. | Concrete copied member variables. |
| `[&]` | Capture all used local variables by reference. | Reference member variables. |
| `[this]` | Capture enclosing class `this` pointer by value. | Raw pointer `EnclosingClass* this;`. |
| `[*this]` (C++17) | Capture enclosing class object **by copy**. | Deep-copied instance `EnclosingClass this_copy;` (Safe for async threads!). |
| `[p = std::move(p)]` (C++14) | Init-capture / Capture by move. | Move-constructed member variable `decltype(p) p;`. |

### 2.1 The `mutable` Keyword
By default, the compiler declares `operator()` on the closure type as `const`. To modify variables captured by copy:

```cpp
int counter = 0;
// ERROR: Cannot modify member 'counter' in const operator()
// auto inc = [counter]() { counter++; }; 

// CORRECT with mutable: Generates non-const `int operator()()`
auto inc = [counter]() mutable { return counter++; };
```

---

## 3. Stateless Lambdas & Function Pointer Conversion

A lambda with an **empty capture list `[]`** is stateless and provides a compiler-generated **implicit conversion operator** to a standard C-style function pointer (`R(*)(Args...)`):

```cpp
auto add = [](int a, int b) { return a + b; };

// Implicit conversion to raw C function pointer:
int (*fp)(int, int) = add;

// Idiomatic unary '+' operator trick to force immediate function pointer decay:
auto fp_forced = +[](int a, int b) { return a + b; };
```

---

## 4. Generic Lambdas vs Template Lambdas

### 4.1 Generic Lambdas (C++14)
Using `auto` in the parameter list generates a **templated `operator()`**:

```cpp
auto print = [](auto x) { std::cout << x << "\n"; };

// Generated:
struct __generic_lambda {
    template <typename T>
    void operator()(T x) const { std::cout << x << "\n"; }
};
```

### 4.2 Explicit Template Lambdas (C++20)
Provides explicit access to the template type `T`:

```cpp
// Direct access to vector element type T without `decltype(vec)::value_type`
auto process_vector = []<typename T>(const std::vector<T>& vec) {
    T sum = 0;
    for (const auto& item : vec) sum += item;
    return sum;
};
```

---

## 5. Type Erasure & `std::function` Internals

`std::function<R(Args...)>` is a **polymorphic type-erased wrapper** capable of storing any callable entity (function pointer, lambda, functor, member function pointer) with matching signature.

```
std::function<void()> Internal Layout (typically 32-48 Bytes):
+-------------------+---------------------------------------------------+
| VTable / Invoker  | Small Buffer Optimization (SBO) Buffer (16-24B)   |
| Function Pointer  | [ Inlined Functor / Lambda (if fits) ]            |
| (8 Bytes)         | OR [ Raw Pointer to Heap Allocated Functor ]      |
+-------------------+---------------------------------------------------+
```

### 5.1 The Cost of `std::function`
1. **Virtual/Indirect Call Overhead**: Invocations dispatch through a function pointer (invoker), preventing inlining.
2. **Small Buffer Optimization (SBO)**: If the captured state exceeds the internal buffer (typically 16–24 bytes), `std::function` performs a **dynamic heap allocation** (`malloc`)!
3. **Cannot store move-only callables**: `std::function` requires the callable to be copy-constructible (cannot store `std::unique_ptr` captures!).

### 5.2 Modern Solutions: `std::move_only_function` (C++23) & `FunctionView`
- **`std::move_only_function` (C++23)**: Supports move-only lambdas (e.g., capturing `std::unique_ptr` or `std::promise`).
- **`FunctionView<R(Args...)>`**: Non-owning callable view (pointer to object + pointer to stub function, zero allocations, 16 bytes).

---

## 6. Recursive Lambdas & Deducing `this` (C++23)

### Pre-C++23: Passing Generic `self` Reference
```cpp
auto fib = [](auto&& self, int n) -> int {
    if (n <= 1) return n;
    return self(self, n - 1) + self(self, n - 2);
};
std::cout << fib(fib, 10); // 55
```

### C++23: Explicit Object Parameter (Deducing `this`)
```cpp
auto fib = [](this auto&& self, int n) -> int {
    if (n <= 1) return n;
    return self(n - 1) + self(n - 2); // Natural direct recursive call!
};
std::cout << fib(10); // 55
```
