# Foundational Questions: Strings & I/O

### Q1: What is the benefit of `std::string_view` over `const std::string&`?
**Answer:** `std::string_view` can point to string literals, sub-strings (`substr()` is O(1)), `char[]` arrays, or `std::string` without triggering temporary string allocation or copying.

### Q2: Is `std::string_view` null-terminated?
**Answer:** Not necessarily! A `string_view` can represent a slice of a string. Passing `.data()` from a non-null-terminated `string_view` into C-style APIs expecting null-termination leads to out-of-bounds reads / UB.
