# Exception Handling & noexcept — Learning Resources

---

### 1. C++ Core Guidelines — Error Handling
https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#e-error-handling

Official guidelines on when to use exceptions, how to throw/catch properly, and RAII's role in exception safety. Covers E.1–E.31.
**Time: 20–25 min**

---

### 2. Back to Basics: Exceptions — CppCon (Klaus Iglberger)
https://www.youtube.com/watch?v=0ojB8c0xUd8

Clear CppCon talk covering exception mechanics, stack unwinding, exception safety guarantees, and noexcept. Best single video.
**Time: 60 min (watch at 1.5x → 40 min)**

---

### 3. Effective Modern C++ — Item 14: Declare functions noexcept if they won't emit exceptions (Scott Meyers)
Book: "Effective Modern C++"

Explains why `noexcept` matters for performance (move operations, vector reallocation) and correctness. Essential reading.
**Time: 15 min**

---

### 4. Exception Safety — Herb Sutter (GotW #59, #60, #61)
https://herbsutter.com/2000/07/01/exception-safety-in-c/

Herb Sutter's classic articles defining the three exception safety guarantees and showing how to achieve them with RAII and copy-and-swap.
**Time: 25 min**

---

### 5. cppreference — noexcept specifier and operator
https://en.cppreference.com/w/cpp/language/noexcept_spec
https://en.cppreference.com/w/cpp/language/noexcept

The definitive reference for `noexcept` syntax, semantics, and the `noexcept` operator (compile-time query).
**Time: 10 min**
