# Theory & Mechanics: I/O, Formatting & Strings

## 1. Small String Optimization (SSO) Memory Layout

`std::string` manages dynamic character data using a dual-mode union memory representation (24 or 32 bytes on 64-bit platforms):

```
1. Short String Mode (SSO Active - 15 char capacity on 64-bit GCC):
   ┌──────────────────────────────────────────────┬────────────┬──────┐
   │ char local_buffer[16] ("Hello World\0")      │ size (11)  │ \0   |
   └──────────────────────────────────────────────┴────────────┴──────┘
   ^ Heap Allocation: NONE (0 syscalls, stored on stack frame)

2. Long String Mode (SSO Inactive):
   ┌──────────────────────┬────────────┬──────────────────────────────┐
   │ char* heap_ptr       │ size (100) │ capacity (128)               │
   └──────────────────────┴────────────┴──────────────────────────────┘
   ^ Points to dynamically allocated heap memory block
```

### Key Performance Rule:
Strings shorter than 15–22 characters incur **zero heap allocations**. Appending beyond SSO threshold triggers dynamic memory reallocation.

---

## 2. Zero-Copy `std::string_view` & POSIX System Call Traps

`std::string_view` (C++17) is a lightweight, non-owning reference consisting of two fields (`16 bytes`):
1. `const char* ptr`
2. `size_t length`

### CRITICAL TRAP 1: Non-Null-Terminated Substrings in POSIX APIs
`std::string_view` does NOT guarantee null-termination (`\0`)!

```cpp
std::string text = "hello world";
std::string_view sub = std::string_view(text).substr(0, 5); // "hello"

// DANGER / UNDEFINED BEHAVIOR:
FILE* f = fopen(sub.data(), "r"); 
// sub.data() points to "hello world\0"! fopen continues reading past "hello" -> File Open Error / Crash!

// FIX: Convert to std::string before passing to C-style APIs:
FILE* f_safe = fopen(std::string(sub).c_str(), "r");
```

### CRITICAL TRAP 2: Returning `string_view` to Temporary
```cpp
std::string_view get_greeting() {
    std::string s = "Hello";
    return s; // BUG: 's' destroyed at return -> Returns Dangling Pointer view!
}
```

---

## 3. Modern Formatting: `std::format` (C++20) & `std::print` (C++23)

`std::format` replaces `printf` and `iostream` with a type-safe, non-allocating format string parser:

### Custom `std::formatter` Specialization:
```cpp
#include <format>
#include <string>
#include <iostream>

struct Point { int x, y; };

template <>
struct std::formatter<Point> : std::formatter<std::string> {
    auto format(const Point& p, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format("Point({}, {})", p.x, p.y), ctx);
    }
};

int main() {
    Point p{10, 20};
    std::cout << std::format("User Location: {}\n", p); // Native formatting!
}
```
