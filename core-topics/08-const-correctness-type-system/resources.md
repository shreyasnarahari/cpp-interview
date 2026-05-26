# Const Correctness & Type System — Learning Resources

---

### 1. C++ Core Guidelines — Constants and Immutability
https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#con-constants-and-immutability

Official guidelines on `const`, `constexpr`, and immutable design. Covers Con.1–Con.5, the foundational rules.
**Time: 15–20 min**

---

### 2. learncpp.com — Const, constexpr, and Symbolic Constants
https://www.learncpp.com/cpp-tutorial/const-variables-and-symbolic-constants/

Beginner-friendly tutorial covering const variables, const references, const pointers, and constexpr. Follow the full const chapter.
**Time: 25–30 min**

---

### 3. Back to Basics: const and constexpr — CppCon (Rainer Grimm)
https://www.youtube.com/watch?v=tA6LbPyYdco

Clear CppCon talk explaining the difference between `const`, `constexpr`, `consteval`, and `constinit` with practical examples.
**Time: 60 min (watch at 1.5x → 40 min)**

---

### 4. Effective C++ — Items 3, 21, 27 (Scott Meyers)
Book: "Effective C++"

- Item 3: Use const whenever possible
- Item 21: Don't try to return a reference when you must return an object
- Item 27: Minimize casting

These three items alone cover 90% of const and cast interview questions.
**Time: 25 min**

---

### 5. cppreference — Type Casting
https://en.cppreference.com/w/cpp/language/static_cast
https://en.cppreference.com/w/cpp/language/dynamic_cast
https://en.cppreference.com/w/cpp/language/const_cast
https://en.cppreference.com/w/cpp/language/reinterpret_cast

The definitive reference for each casting operator. Read the "Notes" and "Explanation" sections — they cover UB edge cases.
**Time: 20 min (skim all four)**

---

### 6. The constexpr/consteval/constinit Evolution
https://www.modernescpp.com/index.php/c-20-consteval-and-constinit

Rainer Grimm's concise comparison of all three compile-time keywords with examples.
**Time: 15 min**
