# Theory & Mechanics: Lambda Expressions & Functional

## 1. Lambda — Compiler-Generated Closure Class

A lambda expression is syntactic sugar for an **anonymous class** with `operator()`:

```cpp
int x = 10;
auto fn = [x](int y) { return x + y; };

// Compiler generates approximately:
class __lambda_1 {
    int x;  // captured by value — stored as member
public:
    __lambda_1(int x) : x(x) {}
    int operator()(int y) const { return x + y; }
    // operator() is const by default — captured values are immutable
};
auto fn = __lambda_1(x);
```

**Key implication:** Each lambda has a **unique, unnamed type**. Two lambdas with identical code have different types. You cannot assign one lambda to a variable holding another:
```cpp
auto a = [](int x) { return x + 1; };
auto b = [](int x) { return x + 1; };
// decltype(a) != decltype(b) — different types!
// a = b;  // ERROR — type mismatch
```

## 2. Capture Mechanics — Value vs Reference

```
Capture by value [x]:
Closure object:
┌──────────────────┐
│ int x (copy)     │  ← independent copy, survives original's destruction
│ operator()       │
└──────────────────┘

Capture by reference [&x]:
Closure object:
┌──────────────────┐
│ int* x_ptr ──────────→ original x (DANGER: dangling if x dies!)
│ operator()       │
└──────────────────┘

Capture by init-capture [ptr = std::move(uptr)]:
Closure object:
┌──────────────────────────┐
│ unique_ptr<T> ptr (moved)│  ← ownership transferred into lambda
│ operator()               │
└──────────────────────────┘
```

### `mutable` keyword:
By default, `operator()` is `const`, so by-value captures cannot be modified:
```cpp
int count = 0;
auto fn = [count]() mutable {  // mutable makes operator() non-const
    count++;  // modifies lambda's internal copy, NOT the original
    return count;
};
fn(); // returns 1
fn(); // returns 2 (state persists between calls)
// count is still 0 — original is unchanged
```

### `[this]` vs `[*this]` (C++17):
```cpp
class Widget {
    int data = 42;
    auto make_fn() {
        return [this] { return data; };   // captures `this` pointer — dangling if Widget dies
        return [*this] { return data; };  // captures copy of entire Widget — safe
    }
};
```

## 3. `std::function` — Type Erasure Internals

`std::function` wraps ANY callable (lambda, function pointer, functor) into a uniform type. It achieves this via **type erasure**:

```
std::function<int(int)> fn = [x](int y) { return x + y; };

std::function object:
┌───────────────────────────────────────────┐
│ Small Buffer (SBO):                       │
│ ┌───────────────────────────────────────┐ │  ← if closure fits (~16-32 bytes),
│ │ closure data stored inline            │ │     stored here (no heap allocation)
│ └───────────────────────────────────────┘ │
│ OR                                        │
│ void* heap_ptr ──→ [closure on heap]      │  ← if closure is too large, heap-allocated
│                                           │
│ invoker_fn_ptr ──→ type-erased call shim  │  ← virtual-dispatch-like indirection
│ manager_fn_ptr ──→ copy/destroy/move ops  │
└───────────────────────────────────────────┘
sizeof(std::function) ≈ 32-48 bytes (implementation-dependent)
```

**Performance implications:**
| Approach | Overhead | Inlineable? |
|---|---|---|
| Lambda as template parameter | Zero — direct call | ✅ Yes |
| `std::function` (small closure) | ~2-5 ns indirect call | ❌ No |
| `std::function` (large closure) | + heap allocation | ❌ No |

**Rule:** Use template parameters for hot paths. Use `std::function` only when you need type erasure (storing heterogeneous callables in a container, virtual interface).

## 4. Generic Lambdas (C++14) & Template Lambdas (C++20)

```cpp
// Generic lambda — auto parameters make operator() a template:
auto add = [](auto a, auto b) { return a + b; };
// Compiler generates:
// template <typename T1, typename T2>
// auto operator()(T1 a, T2 b) const { return a + b; }

// Template lambda (C++20) — explicit template syntax:
auto convert = []<typename T>(std::vector<T>& vec) {
    // T is accessible! Can't do this with auto parameters
    T sum = std::accumulate(vec.begin(), vec.end(), T{});
    return sum;
};
```

## 5. Immediately Invoked Lambda Expression (IILE)

Used for **complex initialization of const variables**:

```cpp
// Without IILE — x can't be const:
int x;
if (condition) x = compute_a();
else x = compute_b();

// With IILE — x is const, initialized in one expression:
const int x = [&] {
    if (condition) return compute_a();
    else return compute_b();
}();  // ← invoked immediately
```

The trailing `()` calls the lambda immediately and discards it. The result is a single, clean initialization expression.
