# Intermediate Questions: Modern Formatting & SSO

### Q1: Why is `std::format` faster than `std::stringstream`?
**Answer:** `std::format` parses format strings at compile time (or efficiently at runtime), uses type-erased argument buffers without locale virtual function overhead, and writes directly into pre-allocated output buffers without intermediate allocations.

### Q2: Write a custom `std::formatter` for a point struct.
```cpp
#include <format>
#include <string>

struct Point { int x; int y; };

template <>
struct std::formatter<Point> : std::formatter<std::string> {
    auto format(const Point& p, format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format("({}, {})", p.x, p.y), ctx);
    }
};
```
