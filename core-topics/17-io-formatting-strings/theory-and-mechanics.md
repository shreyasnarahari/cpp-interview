# Theory & Mechanics: I/O, Formatting & Strings

## 1. Small String Optimization (SSO)

`std::string` uses a dual memory representation (typically 24 or 32 bytes total size on 64-bit platforms):
- **Short String Mode**: Stores characters directly inside the `std::string` object buffer (usually 15-22 chars). `capacity()` is static, heap allocation is avoided.
- **Long String Mode**: Stores pointer `char* data`, `size_t size`, and `size_t capacity`. Allocates memory dynamically via `new`.

```
Short String Layout (GCC libstdc++ 15-byte capacity):
+--------------------+------+------+
| local_buf[16]      | size | \0   |
+--------------------+------+------+

Long String Layout:
+--------------------+------+----------+
| char* heap_ptr     | size | capacity |
+--------------------+------+----------+
```

## 2. `std::string_view` Dangling References

`std::string_view` holds a pointer (`const char*`) and a size (`size_t`). It does not extend the lifetime of temporary strings!

```cpp
// DANGER: Dangling string_view!
std::string_view get_view() {
    std::string s = "temporary";
    return s; // Returns pointer to s which dies at end of function
}
```
