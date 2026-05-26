# Exception Handling & noexcept — Hard Questions

---

**Q1. What is `std::expected<T, E>` (C++23)?**

A: A vocabulary type for returning either a value OR an error — like Rust's `Result<T, E>`:

```cpp
std::expected<int, std::string> parseInt(std::string_view s) {
    try {
        return std::stoi(std::string(s));
    } catch (...) {
        return std::unexpected("invalid integer: " + std::string(s));
    }
}

auto result = parseInt("42");
if (result) {
    std::cout << *result;  // 42
} else {
    std::cerr << result.error();
}

// Monadic operations (C++23):
auto doubled = parseInt("21")
    .transform([](int x) { return x * 2; })     // 42
    .transform_error([](auto e) { return "ERR: " + e; });
```

Advantages over exceptions: no stack unwinding, explicit error handling, zero overhead on success, composable.

---

**Q2. What is the `function-try-block`?**

A: A try/catch that wraps an entire constructor body, including the member initializer list:

```cpp
class Widget {
    std::string name_;
    Database db_;
public:
    Widget(const std::string& name, const std::string& connStr)
    try : name_(name), db_(connStr)  // if db_ ctor throws...
    {
        // constructor body
    }
    catch (const std::exception& e) {
        std::cerr << "Widget construction failed: " << e.what() << "\n";
        // NOTE: the exception is automatically re-thrown!
        // You CANNOT suppress it — the object doesn't exist
    }
};
```

Function-try-blocks are the ONLY way to catch exceptions from member initializer lists.

---

**Q3. What are exception guarantees of standard library operations?**

A: Key examples:

| Operation | Guarantee |
|---|---|
| `vector::push_back` | Strong (if copy/move is noexcept) |
| `vector::insert` | Basic |
| `vector::erase` | No-throw (if element destructor is no-throw) |
| `map::insert` | Strong |
| `map::erase` | No-throw |
| `make_shared`/`make_unique` | Strong (single allocation) |
| `swap` (all containers) | No-throw |
| All destructors | No-throw (implicitly `noexcept`) |
| `std::sort` | Basic (may leave range partially sorted) |

---

## Coding Questions

---

**C1. Exception-safe assignment with strong guarantee**

A:
```cpp
class Image {
    std::vector<uint8_t> pixels_;
    int width_, height_;
public:
    Image& operator=(const Image& other) {
        // Strong guarantee via copy-and-swap:
        Image copy(other);         // if this throws, *this is untouched
        swap(*this, copy);         // noexcept
        return *this;              // old data destroyed in copy's dtor
    }

    friend void swap(Image& a, Image& b) noexcept {
        using std::swap;
        swap(a.pixels_, b.pixels_);
        swap(a.width_, b.width_);
        swap(a.height_, b.height_);
    }
};
```

---

**C2. Scope guard (generic RAII cleanup)**

A:
```cpp
template <typename F>
class ScopeGuard {
    F cleanup_;
    bool active_ = true;
public:
    explicit ScopeGuard(F fn) : cleanup_(std::move(fn)) {}
    ~ScopeGuard() { if (active_) cleanup_(); }
    void dismiss() { active_ = false; }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
};

template <typename F>
ScopeGuard<F> makeScopeGuard(F fn) { return ScopeGuard<F>(std::move(fn)); }

// Usage — guaranteed cleanup:
void process() {
    auto* resource = acquireResource();
    auto guard = makeScopeGuard([&]() { releaseResource(resource); });

    riskyOperation(resource);  // if this throws, guard cleans up
    moreWork(resource);

    guard.dismiss();           // success — don't clean up
    commitResource(resource);
}
```

---

**C3. `std::expected` from scratch (simplified)**

A:
```cpp
template <typename T, typename E>
class Expected {
    std::variant<T, E> data_;
    bool hasValue_;
public:
    Expected(T value) : data_(std::move(value)), hasValue_(true) {}
    Expected(E error) : data_(std::move(error)), hasValue_(false) {}

    explicit operator bool() const { return hasValue_; }
    T& value() { return std::get<T>(data_); }
    E& error() { return std::get<E>(data_); }

    T& operator*() { return value(); }

    template <typename F>
    auto transform(F fn) -> Expected<decltype(fn(value())), E> {
        if (hasValue_) return fn(value());
        return error();
    }
};
```
