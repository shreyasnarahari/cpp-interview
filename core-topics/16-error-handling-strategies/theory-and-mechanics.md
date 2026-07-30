# Theory & Mechanics: Error Handling Strategies & std::expected

## 1. The Error Handling Paradigm Matrix

C++ offers three primary error handling mechanisms, each suited for specific architectural requirements:

| Dimension | Exceptions (`try/catch/throw`) | Error Codes (`std::error_code`) | Monadic (`std::expected<T, E>`) |
|---|---|---|---|
| **Happy Path Cost** | **Zero CPU Overhead** ($0$ instructions) | `if (err)` branching check (~1 cycle) | `if (exp)` status check (~1 cycle) |
| **Sad Path (Error) Cost**| **High** (Stack unwinding, `.eh_frame`) | **Zero** (Returns integer code) | **Zero** (Returns tagged union payload) |
| **Memory Allocation** | May allocate heap object | Zero | Zero (In-place tagged storage) |
| **Interface Explicit** | Implicit (Can be forgotten) | Explicit (Requires return check) | **Explicit & Enforced** |
| **Primary Domain** | Application business logic | System / POSIX APIs | HFT, Embedded, Monadic Chains |

---

## 2. Monadic Error Handling: `std::expected<T, E>` (C++23)

Introduced in C++23, `std::expected<T, E>` is a vocabulary type containing either an expected value `T` or an unexpected error `E` stored in a contiguous, non-allocating tagged union.

```
std::expected<T, E> Memory Layout:
┌───────────────────────────────────────┬────────────┐
│ Storage (Union of T value and E error)│ Tag (bool) │
└───────────────────────────────────────┴────────────┘
```

### Monadic Chaining Operations:
- `.and_then(func)`: Invokes `func` (which returns another `std::expected`) if value is present.
- `.transform(func)`: Applies `func` to value `T` and wraps the result in `std::expected`.
- `.or_else(func)`: Handles the error `E` if present.
- `.value_or(default_val)`: Returns `T` or fallback `default_val`.

```cpp
#include <expected>
#include <string>
#include <iostream>

enum class MathError { DivisionByZero, NegativeLogarithm };

std::expected<double, MathError> divide(double a, double b) {
    if (b == 0.0) return std::unexpected(MathError::DivisionByZero);
    return a / b;
}

std::expected<double, MathError> log_safe(double val) {
    if (val <= 0.0) return std::unexpected(MathError::NegativeLogarithm);
    return std::log(val);
}

// Monadic Pipeline Chaining
std::expected<double, MathError> compute(double a, double b) {
    return divide(a, b).and_then(log_safe);
}
```

---

## 3. Custom System Error Categories (`std::error_code`)

`std::error_code` encapsulates an integer error value and a reference to a singleton `std::error_category` to achieve non-allocating polymorphic error matching across ABI boundaries.

```cpp
#include <system_error>
#include <string>

enum class NetworkError { Success = 0, ConnectionReset, Timeout };

class NetworkErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override { return "NetworkError"; }
    std::string message(int ev) const override {
        switch (static_cast<NetworkError>(ev)) {
            case NetworkError::ConnectionReset: return "Connection reset by peer";
            case NetworkError::Timeout: return "Socket timeout";
            default: return "Success";
        }
    }
};

const NetworkErrorCategory& network_category() {
    static NetworkErrorCategory instance;
    return instance;
}

std::error_code make_error_code(NetworkError e) {
    return {static_cast<int>(e), network_category()};
}
```
