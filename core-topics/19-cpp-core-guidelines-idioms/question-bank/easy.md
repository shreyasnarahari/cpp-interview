# Foundational Questions: Idioms & Core Guidelines

### Q1: What is the Rule of Zero?
**Answer:** The Rule of Zero states that classes that do not manage custom raw resources should rely on standard types (`std::string`, `std::vector`, `std::unique_ptr`) and declare zero copy/move constructors or destructors, allowing the compiler to generate correct ones automatically.

### Q2: Why is `make_unique` preferred over raw `new`?
**Answer:** It prevents resource leaks caused by un-ordered evaluation of function arguments prior to C++17 and cleanly expresses single ownership semantics.
