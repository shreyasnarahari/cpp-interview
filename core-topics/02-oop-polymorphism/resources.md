# OOP & Polymorphism — Learning Resources

Read in this order. Each one builds on the previous.

---

### 1. learncpp.com — Inheritance & Virtual Functions
https://www.learncpp.com/cpp-tutorial/introduction-to-inheritance/

Comprehensive tutorial series covering inheritance, virtual functions, and polymorphism from scratch. Follow the entire chapter (18.x series).
**Time: 45–60 min**

---

### 2. Back to Basics: Virtual Dispatch — CppCon
https://www.youtube.com/watch?v=d0oBBTFbOmY

CppCon talk that visualizes the vtable mechanism, explains when virtual dispatch happens, and covers the constructor/destructor pitfalls.
**Time: 60 min (watch at 1.5x → 40 min)**

---

### 3. C++ Core Guidelines — Class Hierarchies
https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-hier

Official guidelines on inheritance, virtual functions, and class design. Focus on C.35 (virtual destructor), C.67 (non-copyable polymorphic base), and C.120-C.140 sections.
**Time: 20–25 min**

---

### 4. Effective C++ — Items 7, 32–40 (Scott Meyers)
Book: "Effective C++" by Scott Meyers

Key items:
- Item 7: Declare destructors virtual in polymorphic base classes
- Item 32: Make sure public inheritance models "is-a"
- Item 34: Differentiate between inheritance of interface and implementation
- Item 36: Never redefine an inherited non-virtual function
**Time: 30–40 min**

---

### 5. CRTP and Static Polymorphism
https://eli.thegreenplace.net/2011/05/17/the-curiously-recurring-template-pattern-in-c

Excellent explanation of CRTP with practical examples. Shows how to achieve polymorphism without virtual function overhead.
**Time: 20 min**
