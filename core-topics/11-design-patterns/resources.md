# Design Patterns in Modern C++ — Learning Resources

---

### 1. Design Patterns in Modern C++ — Dmitri Nesteruk
Book — covers all classic patterns reimagined with C++17/20 features (templates, smart pointers, lambdas). More relevant than the original GoF book for C++ interviews.
**Time: 2–3 hours (read Creational + select Structural/Behavioral chapters)**

---

### 2. C++ Core Guidelines — Class Design
https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-class

Guidelines on interface design, inheritance, and when to use which pattern. Covers T.1 (use templates for static polymorphism) and C.120–C.140 (class hierarchies).
**Time: 20 min**

---

### 3. Back to Basics: Design Patterns — CppCon (Klaus Iglberger)
https://www.youtube.com/watch?v=2UUqX2eIdSM

Modern take on GoF patterns in C++. Emphasizes that many Java patterns are unnecessary in C++ (replaced by templates, lambdas, RAII).
**Time: 60 min (watch at 1.5x → 40 min)**

---

### 4. Breaking Dependencies: Type Erasure — CppCon (Klaus Iglberger)
https://www.youtube.com/watch?v=jKt6A3wnDyI

Deep dive into type erasure as the modern replacement for OOP hierarchies. Shows `std::function`, `std::any`, and custom type-erased interfaces.
**Time: 60 min (watch at 1.5x → 40 min)**

---

### 5. Policy-Based Design — Andrei Alexandrescu (Modern C++ Design)
Book: "Modern C++ Design" — Chapter 1

The seminal work on policy-based design using templates. Shows how to replace inheritance hierarchies with composable template policies.
**Time: 30 min (Chapter 1 only)**
