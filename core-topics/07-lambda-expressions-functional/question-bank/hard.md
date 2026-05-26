# Lambda Expressions — Hard Questions

---

**Q1. What is a template lambda (C++20)?**

A: A lambda with explicit template parameters:

```cpp
// Generic lambda (C++14) — auto deduction
auto old = [](auto x, auto y) { return x + y; };

// Template lambda (C++20) — explicit template params
auto explicit_types = []<typename T>(std::vector<T>& v) {
    T sum{};
    for (const auto& x : v) sum += x;
    return sum;
};

// Useful when you need the type name (e.g., for static_assert, constraints)
auto constrained = []<std::integral T>(T x) { return x * 2; };
```

---

**Q2. How are lambdas implemented internally?**

A: The compiler generates an unnamed class (closure type) with:
1. `operator()` matching the lambda body
2. Member variables for each captured variable
3. If captureless: implicit conversion to function pointer

```cpp
auto fn = [x, &y](int z) { return x + y + z; };

// Compiler generates roughly:
struct __lambda_42 {
    int x;      // captured by value
    int& y;     // captured by reference

    auto operator()(int z) const {
        return x + y + z;
    }
};
__lambda_42 fn{x, y};
```

---

**Q3. What is the `[*this]` capture (C++17)?**

A: Captures a **copy** of the entire object (not just the `this` pointer):

```cpp
class Widget {
    int value = 42;
public:
    auto getClosure() {
        // [this] — captures pointer, dangerous if Widget is destroyed
        // [*this] — captures copy of Widget, safe
        return [*this]() { return value; };
    }
};

Widget w;
auto fn = w.getClosure();
// w could be destroyed here
fn();  // safe with [*this] — has its own copy
```

---

## Coding Questions

---

**C1. Currying with lambdas**

A:
```cpp
auto curry = [](auto fn) {
    return [fn](auto x) {
        return [fn, x](auto y) {
            return fn(x, y);
        };
    };
};

auto add = [](int a, int b) { return a + b; };
auto add5 = curry(add)(5);
std::cout << add5(3);  // 8
```

---

**C2. Y-combinator for recursive lambdas**

A:
```cpp
template <typename F>
struct YCombinator {
    F fn;
    template <typename... Args>
    auto operator()(Args&&... args) const {
        return fn(*this, std::forward<Args>(args)...);
    }
};

auto fib = YCombinator{[](auto self, int n) -> int {
    if (n <= 1) return n;
    return self(n - 1) + self(n - 2);
}};

std::cout << fib(10);  // 55
```

---

**C3. Pipeline operator with lambdas**

A:
```cpp
template <typename T, typename F>
auto operator|(T&& val, F&& fn) {
    return fn(std::forward<T>(val));
}

auto result = 5
    | [](int x) { return x * 2; }      // 10
    | [](int x) { return x + 3; }      // 13
    | [](int x) { return std::to_string(x); };  // "13"
```

---

**C4. Higher-order filter and map**

A:
```cpp
template <typename Pred>
auto filter(Pred pred) {
    return [pred](const auto& container) {
        using T = typename std::decay_t<decltype(container)>::value_type;
        std::vector<T> result;
        std::copy_if(container.begin(), container.end(),
                     std::back_inserter(result), pred);
        return result;
    };
}

template <typename F>
auto map(F fn) {
    return [fn](const auto& container) {
        using R = decltype(fn(*container.begin()));
        std::vector<R> result;
        std::transform(container.begin(), container.end(),
                       std::back_inserter(result), fn);
        return result;
    };
}

std::vector<int> nums = {1, 2, 3, 4, 5, 6};
auto result = nums
    | filter([](int x) { return x % 2 == 0; })
    | map([](int x) { return x * x; });
// result = {4, 16, 36}
```
