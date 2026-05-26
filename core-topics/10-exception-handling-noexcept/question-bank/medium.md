# Exception Handling & noexcept — Medium Questions

---

**Q1. What are the three exception safety guarantees?**

A:

| Guarantee | Promise | Example |
|---|---|---|
| **No-throw** | Function never throws. Marked `noexcept`. | Destructors, `swap`, move operations |
| **Strong** | If exception occurs, state is rolled back (commit-or-rollback). | Copy-and-swap assignment, `std::vector::push_back` |
| **Basic** | If exception occurs, invariants are preserved, no leaks, but state may be modified. | Most standard library operations |
| *(No guarantee)* | *(Avoid — leaks, corrupt state possible)* | *(Legacy code without RAII)* |

```cpp
// Strong guarantee via copy-and-swap:
Widget& Widget::operator=(Widget other) {  // copy made in parameter
    swap(*this, other);                     // noexcept swap
    return *this;                           // old data destroyed in other's dtor
}
// If the copy (in parameter) throws → *this is untouched → strong guarantee
```

---

**Q2. Why must destructors never throw?**

A: If a destructor throws during **stack unwinding** (while handling another exception), `std::terminate()` is called — the program crashes.

```cpp
struct Bad {
    ~Bad() noexcept(false) {
        throw std::runtime_error("dtor error");
    }
};

void f() {
    try {
        Bad b;
        throw std::runtime_error("body error");
        // Stack unwinding: ~Bad() is called → throws → std::terminate()
    } catch (...) {
        // Never reached
    }
}
```

**Rule:** Destructors are implicitly `noexcept`. If a destructor must handle errors, catch and swallow them internally:
```cpp
~Connection() noexcept {
    try { close(); } catch (...) { /* log and swallow */ }
}
```

---

**Q3. How does `std::vector` use `noexcept` during reallocation?**

A: When `push_back` triggers reallocation, vector needs to move elements to the new buffer. But if a move constructor throws mid-transfer:
- Elements already moved → source buffer is corrupted (moved-from)
- Cannot rollback → violates strong exception guarantee

**Solution:** `std::vector` checks `noexcept`:
- Move ctor is `noexcept` → **moves** elements (fast)
- Move ctor is NOT `noexcept` → **copies** elements (safe but slow, can rollback)

```cpp
struct Fast {
    Fast(Fast&&) noexcept = default;  // vector MOVES — O(1) per element
};

struct Slow {
    Slow(Slow&&) { /* might throw */ }  // vector COPIES — O(n) per element
};
```

Internally, `std::vector` uses `std::move_if_noexcept`:
```cpp
// Simplified:
template <typename T>
auto move_if_noexcept(T& x) {
    if constexpr (std::is_nothrow_move_constructible_v<T>)
        return std::move(x);
    else
        return x;  // returns lvalue → copy
}
```

---

**Q4. Exceptions vs error codes — when to use which?**

A:

| | Exceptions | Error codes |
|---|---|---|
| Use when | Errors are rare, normal path is fast | Errors are expected/frequent |
| Performance | Zero cost on success; expensive on throw | Small cost on every call (check) |
| Can be ignored? | No — propagates up automatically | Yes — caller can forget to check |
| Cleanup | Stack unwinding + RAII | Manual or RAII |
| Style | Bloomberg, most C++ codebases | Google (limited exceptions), C interop |

**Google style:** "We do not use C++ exceptions" — primarily for historical/codebase consistency reasons. New code at Google uses `absl::Status` / `absl::StatusOr<T>`.

**C++23:** `std::expected<T, E>` — Rust-like `Result` type, best of both worlds:
```cpp
std::expected<int, std::string> parseInt(std::string_view s) {
    int val;
    if (/* parse fails */) return std::unexpected("not a number");
    return val;
}
```

---

## Coding Questions

---

**C1. Exception-safe `push_back` (strong guarantee)**

A:
```cpp
template <typename T>
class DynArray {
    T* data_;
    size_t size_, capacity_;
public:
    void push_back(const T& value) {
        if (size_ == capacity_) {
            size_t newCap = capacity_ ? capacity_ * 2 : 1;
            T* newData = static_cast<T*>(::operator new(newCap * sizeof(T)));

            // Move (or copy) existing elements
            size_t i = 0;
            try {
                for (; i < size_; i++) {
                    new (newData + i) T(std::move_if_noexcept(data_[i]));
                }
                new (newData + size_) T(value);  // copy new element
            } catch (...) {
                // Rollback: destroy any elements we already moved/copied
                for (size_t j = 0; j < i; j++) {
                    (newData + j)->~T();
                }
                ::operator delete(newData);
                throw;  // re-throw, original array is untouched
            }

            // Success — destroy old and swap
            for (size_t j = 0; j < size_; j++) (data_ + j)->~T();
            ::operator delete(data_);
            data_ = newData;
            capacity_ = newCap;
        } else {
            new (data_ + size_) T(value);
        }
        size_++;
    }
};
```

---

**C2. Exception-safe swap (no-throw guarantee)**

A:
```cpp
class Widget {
    int* data_;
    size_t size_;
public:
    friend void swap(Widget& a, Widget& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);    // pointer swap — can't throw
        swap(a.size_, b.size_);    // integer swap — can't throw
    }

    Widget& operator=(Widget other) {  // copy-and-swap
        swap(*this, other);             // noexcept
        return *this;
    }
};
```

---

**C3. Transport exceptions across threads**

A:
```cpp
std::exception_ptr threadException;

void worker() {
    try {
        riskyWork();
    } catch (...) {
        threadException = std::current_exception();  // capture
    }
}

int main() {
    std::thread t(worker);
    t.join();
    if (threadException) {
        try {
            std::rethrow_exception(threadException);  // re-throw in main thread
        } catch (const std::exception& e) {
            std::cerr << "Worker failed: " << e.what() << "\n";
        }
    }
}
```

---

**C4. `noexcept` operator for conditional noexcept**

A:
```cpp
template <typename T>
class Container {
    T* data_;
    size_t size_;
public:
    // This method is noexcept only if T's destructor is noexcept
    void clear() noexcept(std::is_nothrow_destructible_v<T>) {
        for (size_t i = 0; i < size_; i++) data_[i].~T();
        size_ = 0;
    }

    // Move constructor is noexcept only if T's move is noexcept
    Container(Container&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }
};
```
