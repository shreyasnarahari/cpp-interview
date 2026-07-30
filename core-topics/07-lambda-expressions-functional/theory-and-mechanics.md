# Theory & Mechanics: Lambda Expressions & Functional Programming

## 1. Compiler Generation of Anonymous Closure Classes

When you write a lambda expression, the compiler synthesizes a unique, unnamed **closure class** behind the scenes.

### Transformation Example:
```cpp
int factor = 2;
auto multiply = [factor](int val) mutable {
    factor += 1;
    return val * factor;
};
```

### Compiler Generated Class Equivalent:
```cpp
class __Lambda_Unique_ID {
    int factor; // Captured member
public:
    __Lambda_Unique_ID(int f) : factor(f) {}

    // 'mutable' removes 'const' from operator()
    int operator()(int val) {
        factor += 1;
        return val * factor;
    }
};

__Lambda_Unique_ID multiply(factor);
```

### Key Language Rules:
1. **`operator()` is `const` by default**: Captured-by-value members cannot be modified unless the lambda is explicitly declared **`mutable`**.
2. **Decay to Function Pointer**: A **stateless** lambda (capturing zero variables) generates an implicit conversion operator to a C-style function pointer (`using FP = R(*)(Args...)`). Stateful lambdas cannot decay to function pointers!
3. **Unique Types**: Every lambda expression has a distinct, un-namable type created by the compiler. You **cannot assign** one lambda to another, even if their signatures match!

---

## 2. Capture Modes Taxonomy & Mechanics

| Capture Syntax | Mechanism | Memory / Lifetime Implications |
|---|---|---|
| `[x]` | Value | Copies `x` into closure object. Immutable inside lambda unless `mutable`. |
| `[&x]` | Reference | Stores `x*` pointer inside closure. **Dangling risk** if `x` leaves scope! |
| `[=]` | Value (All) | Copies all referenced outer local variables by value. |
| `[&]` | Reference (All) | References all outer local variables by reference. |
| `[this]` | Pointer Value | Copies `this` pointer by value. Accesses outer object members directly. |
| `[*this]` (C++17) | Object Copy | Copies entire `*this` object into closure by value (safe for async/threads). |
| `[ptr = std::move(p)]` (C++14) | Move Init | Moves non-copyable objects (`unique_ptr`) into closure storage. |

---

## 3. `std::function` vs Template Lambdas (Type Erasure vs Inlining)

### `std::function<R(Args...)>`
- **Mechanism**: Polymorphic type-erasing wrapper. Uses virtual dispatch and a Small Buffer Optimization (SBO, typically 16–32 bytes).
- **Overhead**: Allocates heap memory if closure size exceeds SBO threshold. Calls function via indirect function pointer ($O(1)$ dereference, prevents compiler inlining).

### Template Function Parameter (`template <typename F>`)
- **Mechanism**: Generates specialized code at compile time for the specific closure class type.
- **Overhead**: **Zero overhead**. Fully inlined by the compiler, turning the lambda execution into raw inline instructions!

```cpp
// Fast (Inlined, 0 overhead):
template <typename F>
void run_fast(F&& func) { func(); }

// Slow (Type erased, indirect call, potential heap allocation):
void run_slow(const std::function<void()>& func) { func(); }
```

---

## 4. Modern Lambda Features (C++14 / C++17 / C++20)

### 1. Generic Lambdas (C++14)
Accepts `auto` parameters. The generated `operator()` is a template member function:
```cpp
auto print = [](auto x) { std::cout << x << "\n"; };
// Synthesizes: template <typename T> void operator()(T x) const;
```

### 2. Immediately Invoked Lambda Expressions (IILE)
Executes a lambda at point of declaration to complex-initialize `const` variables:
```cpp
const Config config = [&]() {
    Config cfg;
    if (use_file) cfg.load("settings.json");
    else cfg.load_defaults();
    return cfg; // Returns and immutably assigns to 'config'
}(); // Immediately invoked!
```

### 3. Template Lambdas (C++20)
Provides explicit template parameter syntax for typing constraints:
```cpp
auto process_vector = []<typename T>(const std::vector<T>& vec) {
    std::cout << "Vector size: " << vec.size() << "\n";
};
```

---

## 5. Code Traps & Interview Gotchas

### Code Trap 1: Mutable Lambda Value Isolation
```cpp
int x = 10;
auto fn = [x]() mutable {
    x += 5;
    std::cout << x << " ";
};
fn(); // Prints 15 (modifies closure's internal copy of x)
fn(); // Prints 20 (modifies closure's internal copy of x)
std::cout << x; // Prints 10 (outer x is unaffected!)
// OUTPUT: 15 20 10
```

### Code Trap 2: Reference Capture Lifetime Trap (Dangling Ref)
```cpp
std::function<void()> createTimer() {
    std::string msg = "timeout!";
    return [&msg]() { std::cout << msg; }; // BUG: 'msg' destroyed at function return!
}
// Calling returned function causes Dangling Reference UB!
// FIX: Use value capture [msg] or init capture [msg = std::move(msg)].
```

### Code Trap 3: Lambda Re-assignment Failure
```cpp
auto fn = [](int a, int b) { return a + b; };
// fn = [](int a, int b) { return a * b; }; // COMPILATION ERROR!
// Reasoning: Each lambda has a distinct, unassignable closure type!
// FIX: Use std::function<int(int, int)> fn = ... if reassignment is needed.
```

### Code Trap 4: Loop Reference Capture Trap
```cpp
std::vector<std::function<void()>> fns;
for (int i = 0; i < 5; i++) {
    fns.push_back([&i]() { std::cout << i << " "; }); // BUG: Captures reference to 'i'!
}
for (auto& f : fns) f(); 
// OUTPUT: 5 5 5 5 5 (Because 'i' reached 5 at end of loop!)
// FIX: Capture by value [i]!
```

---

## 6. Implementation Pattern: Functional Compose & Memoization

```cpp
#include <unordered_map>
#include <functional>

// Compose two functions: compose(f, g)(x) = f(g(x))
template <typename F, typename G>
auto compose(F f, G g) {
    return [f, g](auto&& x) {
        return f(g(std::forward<decltype(x)>(x)));
    };
}

// Memoize wrapper using unordered_map and lambdas
template <typename R, typename Arg>
auto memoize(R (*func)(Arg)) {
    return [func](Arg arg) mutable {
        static std::unordered_map<Arg, R> cache;
        auto it = cache.find(arg);
        if (it != cache.end()) return it->second;
        R result = func(arg);
        cache[arg] = result;
        return result;
    };
}
```
