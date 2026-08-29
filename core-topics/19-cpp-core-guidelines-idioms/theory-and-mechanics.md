# C++ Core Guidelines & Idioms — Deep Theory & Mechanics

An exhaustive guide to the Bjarne Stroustrup and Herb Sutter C++ Core Guidelines, essential production idioms (RAII, Copy-and-Swap, Pimpl, NVI, ScopeGuard, Type Erasure), parameter passing rules, and resource ownership models.

---

## 1. The Core Philosophy of the C++ Core Guidelines

| Principle | Guideline Statement | Engineering Impact |
|---|---|---|
| **P.1** | *Express ideas directly in code.* | Use `std::span` over `(ptr, len)`; use `std::unique_ptr` over raw owning pointer. |
| **P.2** | *Write in ISO Standard C++.* | Avoid non-standard compiler extensions for portability. |
| **P.3** | *Express intent.* | Use `const` variables, `const` methods, `[[nodiscard]]`, `override`. |
| **P.4** | *Ideally, a program should be statically type safe.* | Eliminate casts, raw `union`s, and `void*` in favor of `std::variant` and templates. |
| **P.5** | *Prefer compile-time over run-time checks.* | Maximize `static_assert`, `constexpr`, and Concepts. |
| **P.8** | *Don't leak any resources.* | Manage all resource lifetimes through deterministic RAII containers. |

---

## 2. Resource Management Guidelines (R-Series)

- **R.1**: *Store dynamic objects in RAII containers (`unique_ptr`, `shared_ptr`, `vector`).*
- **R.11**: *Avoid calling `malloc`/`free` directly in application code.*
- **R.20**: *Use `std::unique_ptr` or `std::shared_ptr` to represent ownership.*
- **R.21**: *Prefer `unique_ptr` over `shared_ptr` unless resource sharing is truly required.*

---

## 3. The Parameter Passing Decision Matrix (F-Series)

```
                            PARAMETER PASSING DECISION TREE
                                         |
                       Is the parameter an Input, Output, or In-Out?
                                  /            \
                                 /              \
                              INPUT           IN-OUT / OUTPUT
                              /                    \
            Is the type cheap to copy?              Use `T&` (In-Out) or
             (e.g., sizeof(T) <= 2*sizeof(void*)    Return by Value (Output)!
              and trivial copy constructor)
                  /            \
                YES             NO
                /                \
          Pass by VALUE     Pass by CONST REFERENCE
           (`void f(int)`)      (`void f(const Heavy& obj)`)
```

---

## 4. Production C++ Idioms

### 4.1 Scope Guard (Rollback / Cleanup on Exit)

```cpp
template <typename F>
class ScopeGuard {
    F cleanup_;
    bool dismissed_{false};
public:
    explicit ScopeGuard(F f) : cleanup_(std::move(f)) {}
    ~ScopeGuard() { if (!dismissed_) cleanup_(); }
    void dismiss() noexcept { dismissed_ = true; }
};
```

### 4.2 Non-Virtual Interface (NVI) Idiom
Public member functions are **non-virtual** (enforcing pre/post-conditions), while customization points are **private virtual**:

```cpp
class ParserBase {
public:
    void parse(std::string_view data) {
        log_start();
        do_parse(data); // Calls private virtual implementation!
        log_finish();
    }
private:
    virtual void do_parse(std::string_view data) = 0; // Customization hook
};
```
