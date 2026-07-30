# Intermediate Questions: std::error_code & Monadic Operations

### Q1: How does `std::error_code` work under the hood?
**Answer:** `std::error_code` contains a numeric value (`int`) and a pointer to a singleton `std::error_category`. This avoids dynamic allocation while enabling polymorphic error domain matching across module boundaries.

### Q2: Write a chain of operations using `std::expected`.
```cpp
#include <expected>
#include <string>

std::expected<int, std::string> parse_int(std::string_view s);
std::expected<int, std::string> double_int(int val) { return val * 2; }

auto res = parse_int("21").and_then(double_int);
```
