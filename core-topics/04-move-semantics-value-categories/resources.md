# Move Semantics & Value Categories — Learning Resources

---

### 1. Back to Basics: Move Semantics — Klaus Iglberger (CppCon)
https://www.youtube.com/watch?v=knEaMpytRMA

The best single talk on move semantics. Covers lvalues, rvalues, move constructors, std::move, and common pitfalls. Essential viewing.
**Time: 60 min (watch at 1.5x → 40 min)**

---

### 2. Effective Modern C++ — Items 23–30 (Scott Meyers)
Book: "Effective Modern C++"

Items 23–30 cover move semantics comprehensively:
- Item 23: Understand std::move and std::forward
- Item 24: Distinguish universal (forwarding) references from rvalue references
- Item 25: Use std::move on rvalue references, std::forward on forwarding references
- Item 29: Assume move operations are not present, not cheap, and not used
**Time: 45–60 min**

---

### 3. Understanding Value Categories — cppreference
https://en.cppreference.com/w/cpp/language/value_category

The formal specification of value categories (lvalue, prvalue, xvalue, glvalue, rvalue). Dense but precise.
**Time: 15 min (skim the taxonomy diagram)**

---

### 4. A Brief Introduction to Rvalue References — Thomas Becker
http://thbecker.net/articles/rvalue_references/section_01.html

Classic multi-part article that builds up move semantics from first principles. Each section is short and builds on the last.
**Time: 30–40 min**

---

### 5. Copy Elision and Guaranteed Copy Elision (C++17)
https://en.cppreference.com/w/cpp/language/copy_elision

When and how the compiler can skip copy/move operations entirely. C++17 made some cases mandatory.
**Time: 15 min**
