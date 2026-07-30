# Theory & Mechanics: Error Handling Strategies

## 1. Exception Unwinding vs Return Codes

Modern C++ compilers implement zero-cost exception handling (Itanium ABI):
- **Happy Path**: Zero CPU instruction overhead. No `if` checks are performed after function calls.
- **Sad Path (Throw)**: Highly expensive stack unwinding. The runtime parses the `.eh_frame` exception tables, locates landing pads, calls destructors in reverse order, and allocates exception objects on the heap.

## 2. Monadic Error Handling (`std::expected<T, E>`)

Introduced in C++23, `std::expected<T, E>` stores either a value of type `T` or an error of type `E` in a `std::variant`-like tagged union without heap allocation.

### Monadic Operations
- `.and_then()`: Chain operations that return another `std::expected`.
- `.transform()`: Apply a function to the value if present.
- `.or_else()`: Handle the error if present.

```cpp
#include <expected>
#include <string>

enum class ParseError { InvalidChar, Overflow };

std::expected<int, ParseError> parse_number(std::string_view str) {
    if (str.empty()) return std::unexpected(ParseError::InvalidChar);
    // Parse logic...
    return 42;
}
```
