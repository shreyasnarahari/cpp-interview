# I/O, Formatting & String Manipulation — Deep Theory & Mechanics

An exhaustive, performance-oriented guide to C++ string representations (Small String Optimization SSO), non-owning string views (`std::string_view`), fast formatting (`std::format`, `std::print`), custom `std::formatter` specializations, and low-level number conversions (`std::to_chars`/`from_chars`).

---

## 1. String Memory Architectures: `std::string` & SSO

### 1.1 Small String Optimization (SSO) Internals

To eliminate dynamic heap allocations for short strings, `std::string` uses a **discriminated union**:

```
std::string Layout (32 Bytes on 64-bit platforms):
+-------------------------------------------------------------------------------+
| SHORT STRING MODE (Length <= 15 or 23 chars): ZERO HEAP ALLOCATION!           |
|                                                                               |
|  - char buffer_[16 or 24]: Inline character array holding the string directly|
|  - uint8_t size_: Stored inside the buffer or trailing byte                   |
+-------------------------------------------------------------------------------+
| LONG STRING MODE (Length > 15 or 23 chars): HEAP ALLOCATED                    |
|                                                                               |
|  - char* ptr_             (8 Bytes): Points to heap buffer                    |
|  - size_t size_           (8 Bytes): Current string length                    |
|  - size_t capacity_       (8 Bytes): Allocated buffer capacity                |
+-------------------------------------------------------------------------------+
```

---

## 2. `std::string_view`: Non-Owning String Slice (C++17)

`std::string_view` consists of exactly **two words** (16 bytes):
1. Pointer to character buffer (`const char* data_`).
2. Length of the view (`size_t size_`).

### 2.1 Critical Lifetime Gotchas
1. **Not Guaranteed to be Null-Terminated**: Passing `sv.data()` to legacy C APIs (`fopen`, `printf("%s")`) is **Undefined Behavior** unless the source buffer is guaranteed null-terminated!
2. **Dangling String Views**: A `string_view` does not extend the lifetime of the underlying string:
   ```cpp
   std::string_view bad_view = std::string("temporary"); // DANGLING! Temporary string destroyed!
   ```

---

## 3. Fast Formatting: `std::format` (C++20) & `std::print` (C++23)

Why `std::format` outperforms `std::stringstream` and `sprintf`:
1. **Compile-Time Format String Checking**: Format specifiers are validated at compile-time via `consteval`.
2. **Direct Buffer Appends**: Writes directly into pre-allocated memory without intermediate allocations.
3. **Type Safety without VTables**: Zero virtual dispatch overhead compared to streams.

### 3.1 Custom `std::formatter` Specialization

```cpp
#include <format>
#include <iostream>

struct Order {
    uint64_t id;
    double price;
    uint32_t qty;
};

template <>
struct std::formatter<Order> : std::formatter<string_view> {
    template <typename FormatContext>
    auto format(const Order& o, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "Order[#{} Price=${:.2f} Qty={}]", o.id, o.price, o.qty);
    }
};
```

---

## 4. Ultra-Fast Numeric Conversion: `std::to_chars` & `std::from_chars`

- **Locale-Independent**: Never touches slow `std::locale` or global locks.
- **Zero Allocations**: Writes directly into caller-provided char buffers.
- **Dragonbox / Ryu Algorithm**: Implements optimal shortest round-trip floating-point string conversions.

```cpp
#include <charconv>

char buffer[32];
int value = 123456789;

// Fast integer to string:
auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value);
if (ec == std::errc()) {
    std::string_view sv(buffer, ptr - buffer);
    std::cout << "Formatted: " << sv << "\n";
}
```
