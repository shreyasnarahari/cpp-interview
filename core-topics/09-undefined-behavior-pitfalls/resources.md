# Undefined Behavior & Common Pitfalls — Learning Resources

---

### 1. What Every C Programmer Should Know About Undefined Behavior — Chris Lattner (LLVM Blog)
https://blog.llvm.org/2011/05/what-every-c-programmer-should-know.html

Three-part series by the creator of LLVM/Clang. Explains why UB exists, how compilers exploit it, and how it can cause "time travel." The single most important UB article.
**Time: 30 min (all 3 parts)**

---

### 2. CppCon: Undefined Behavior Is Not An Error — Barbara Geller & Ansel Sermersheim
https://www.youtube.com/watch?v=XEXpwis_deQ

Practical CppCon talk cataloging the most common UB sources in real C++ code with live demos.
**Time: 60 min (watch at 1.5x → 40 min)**

---

### 3. cppreference — Undefined Behavior
https://en.cppreference.com/w/cpp/language/ub

Definitive reference listing all forms of undefined behavior in C++. Dense but comprehensive.
**Time: 15 min (skim the categories)**

---

### 4. Sanitizers — Clang Documentation
https://clang.llvm.org/docs/AddressSanitizer.html
https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
https://clang.llvm.org/docs/ThreadSanitizer.html

How to use ASan, UBSan, and TSan to detect UB at runtime. Essential practical tools.
**Time: 15 min (skim usage for each)**

---

### 5. Effective Modern C++ — Item 42 (Scott Meyers)
"Consider emplacement instead of insertion" — covers a subtle UB case with exception safety and container operations.
**Time: 10 min**

---

### 6. A Guide to Undefined Behavior in C and C++ — John Regehr (University of Utah)
https://blog.regehr.org/archives/213

Three-part academic blog series on UB with clear taxonomy and real-world examples of compiler exploitation.
**Time: 25 min**
