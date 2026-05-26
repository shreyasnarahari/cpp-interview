# Lambda Expressions — Medium Questions

---

**Q1. Lambda vs `std::function` — what's the cost?**

A: A lambda is a **unique unnamed type** — the compiler can inline it. `std::function` is **type-erased** with overhead:

| | Lambda (template param) | `std::function` |
|---|---|---|
| Inlining | Yes (compiler knows exact type) | No (virtual dispatch) |
| Allocation | None | Possible heap allocation |
| Call cost | ~1ns (direct) | ~30ns (indirect) |
| Copyable | Depends | Always copyable |
| Type erasure | No | Yes |

```cpp
// Prefer template parameter:
template <typename F>
void forEach(const std::vector<int>& v, F fn) {
    for (int x : v) fn(x);
}

// Only use std::function when you NEED type erasure:
class EventSystem {
    std::unordered_map<std::string, std::function<void()>> handlers_;
public:
    void on(const std::string& event, std::function<void()> handler) {
        handlers_[event] = std::move(handler);
    }
};
```

---

**Q2. What is the dangling reference problem with lambdas?**

A: Capturing by reference (&) can create dangling references if the lambda outlives the captured variable:

```cpp
std::function<int()> createCounter() {
    int count = 0;
    return [&count]() { return ++count; };  // BUG: count destroyed after return
}
auto counter = createCounter();
counter();  // UB — count is dangling
```

Fix: capture by value (or init capture with move):
```cpp
std::function<int()> createCounter() {
    int count = 0;
    return [count]() mutable { return ++count; };  // OK — owns copy
}
```

---

**Q3. Why can't you assign one lambda to another?**

A: Each lambda has a **unique type** — even if they have the same signature:

```cpp
auto a = [](int x) { return x + 1; };
auto b = [](int x) { return x * 2; };
// a = b;  // ERROR — different types

// Fix: use std::function
std::function<int(int)> fn = a;
fn = b;  // OK — type-erased
```

Exception: **captureless** lambdas can convert to function pointers:
```cpp
int (*fp)(int) = [](int x) { return x + 1; };
```

---

## Coding Questions

---

**C1. Memoize wrapper**

A:
```cpp
template <typename F>
auto memoize(F fn) {
    using ResultType = decltype(fn(0));  // simplified for int→R
    std::unordered_map<int, ResultType> cache;

    return [fn, cache = std::move(cache)](int arg) mutable -> ResultType {
        auto it = cache.find(arg);
        if (it != cache.end()) return it->second;
        auto result = fn(arg);
        cache[arg] = result;
        return result;
    };
}

auto fib = memoize([](int n) -> int {
    // Note: this doesn't recursively memoize
    if (n <= 1) return n;
    return n;  // simplified
});
```

---

**C2. Compose functions**

A:
```cpp
template <typename F, typename G>
auto compose(F f, G g) {
    return [f, g](auto&&... args) {
        return f(g(std::forward<decltype(args)>(args)...));
    };
}

auto doubleIt = [](int x) { return x * 2; };
auto addOne = [](int x) { return x + 1; };
auto doubleThenAdd = compose(addOne, doubleIt);
std::cout << doubleThenAdd(5);  // (5*2)+1 = 11
```

---

**C3. Callback registry**

A:
```cpp
class EventEmitter {
    std::unordered_map<std::string, std::vector<std::function<void(int)>>> listeners_;
public:
    void on(const std::string& event, std::function<void(int)> callback) {
        listeners_[event].push_back(std::move(callback));
    }

    void emit(const std::string& event, int data) {
        if (auto it = listeners_.find(event); it != listeners_.end()) {
            for (auto& cb : it->second) cb(data);
        }
    }
};

EventEmitter emitter;
emitter.on("click", [](int x) { std::cout << "Clicked at " << x << "\n"; });
emitter.on("click", [](int x) { std::cout << "Also: " << x << "\n"; });
emitter.emit("click", 42);
```

---

**C4. Loop variable capture fix**

```cpp
// BUG: all print 5 (capture by reference to loop variable)
std::vector<std::function<void()>> fns;
for (int i = 0; i < 5; i++) {
    fns.push_back([&i]() { std::cout << i << " "; });
}
for (auto& f : fns) f();  // "5 5 5 5 5"

// FIX: capture by value
for (int i = 0; i < 5; i++) {
    fns.push_back([i]() { std::cout << i << " "; });
}
for (auto& f : fns) f();  // "0 1 2 3 4"
```
