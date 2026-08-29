# Error Handling Strategies & Modern Error Models — Deep Theory & Mechanics

An exhaustive guide to C++ error handling paradigms: Exceptions, Error Codes (`std::error_code`), Optional Values (`std::optional`), and Modern Functional Expected Monads (`std::expected` / `Result<T, E>`).

---

## 1. Error Handling Taxonomy & Engineering Tradeoffs

| Error Strategy | Best Use Case | Performance Characteristics | Cons / Limitations |
|---|---|---|---|
| **Exceptions (`throw/catch`)** | Truly exceptional conditions (I/O failures, bad alloc, invariant violations). | **Zero cost on happy path**; very high latency ($1-10\mu\text{s}$) on throw path. | Non-deterministic latency (unusable in hard real-time / HFT hot paths); hidden control flow. |
| **`std::error_code` (System)** | OS/System API errors (networking, filesystem). | Very low overhead (returns integer + category pointer). | Verbose; requires manual propagating and error code mapping. |
| **`std::optional<T>`** | Expected absence of value (cache miss, key not found). | Inlined value + boolean flag; zero heap allocation. | Carries no diagnostic information explaining *why* the operation failed. |
| **`std::expected<T, E>` (C++23)**| Expected domain failures (validation, parsing, business logic). | **Deterministic, zero-overhead value semantics**; inlined storage. | Requires C++23 (or custom monad). |

---

## 2. Modern Functional Error Handling: `std::expected` (C++23)

`std::expected<T, E>` holds either a valid value `T` or an unexpected error `E`:

```cpp
#include <expected>
#include <string>

enum class ParseError { EmptyInput, InvalidDigit, Overflow };

std::expected<int, ParseError> parse_integer(std::string_view str) {
    if (str.empty()) return std::unexpected(ParseError::EmptyInput);
    int result = 0;
    for (char c : str) {
        if (c < '0' || c > '9') return std::unexpected(ParseError::InvalidDigit);
        result = result * 10 + (c - '0');
    }
    return result;
}
```

### 2.1 Monadic Composition (`and_then`, `transform`, `or_else`)
Eliminates nested `if-else` boilerplate by chaining operations:

```cpp
auto process_input(std::string_view raw) {
    return parse_integer(raw)
        .and_then([](int id) -> std::expected<User, ParseError> {
            return fetch_user(id); // Returns expected<User, ParseError>
        })
        .transform([](const User& u) {
            return u.email; // Transforms User to string
        });
}
```

---

## 3. Pre-C++23 Custom `Result<T, E>` Implementation

```cpp
template <typename T, typename E>
class Result {
    std::variant<T, E> storage_;
public:
    Result(T val) : storage_(std::in_place_type<T>, std::move(val)) {}
    Result(std::unexpected<E> err) : storage_(std::in_place_type<E>, std::move(err.value())) {}

    [[nodiscard]] bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] T& value() { return std::get<T>(storage_); }
    [[nodiscard]] const T& value() const { return std::get<T>(storage_); }
    [[nodiscard]] const E& error() const { return std::get<E>(storage_); }
};
```
