# Memory Management & RAII — Learning Resources

Read in this order. Each one builds on the previous.

---

### 1. C++ Core Guidelines — Resource Management (Start Here)
https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#r-resource-management

The official C++ Core Guidelines section on resource management. Written by Bjarne Stroustrup and Herb Sutter. Covers RAII philosophy, smart pointer usage rules, and ownership semantics.
**Time: 25–30 min**

---

### 2. Back to Basics: RAII and the Rule of Zero — Arthur O'Dwyer (CppCon)
https://www.youtube.com/watch?v=7Qgd9B1KuMQ

CppCon talk explaining RAII from first principles. Covers Rule of Three/Five/Zero with clear examples. Best single video on the topic.
**Time: 60 min (watch at 1.5x → 40 min)**

---

### 3. Smart Pointers — cppreference
https://en.cppreference.com/w/cpp/memory/unique_ptr
https://en.cppreference.com/w/cpp/memory/shared_ptr
https://en.cppreference.com/w/cpp/memory/weak_ptr

The definitive reference for smart pointer APIs. Read the "Notes" sections — they explain the control block, thread safety guarantees, and custom deleter mechanics.
**Time: 20 min (skim the API, read the Notes)**

---

### 4. GotW #89: Smart Pointers — Herb Sutter
https://herbsutter.com/2013/05/29/gotw-89-solution-smart-pointers/

Herb Sutter's definitive article on when to use which smart pointer. The decision flowchart is invaluable:
- Unique ownership → unique_ptr
- Shared ownership → shared_ptr
- Non-owning observer → raw pointer or weak_ptr
**Time: 15 min**

---

### 5. Effective Modern C++ — Items 18–22 (Scott Meyers)
Book: "Effective Modern C++" by Scott Meyers

Items 18–22 cover smart pointers in depth:
- Item 18: Use unique_ptr for exclusive-ownership
- Item 19: Use shared_ptr for shared-ownership
- Item 20: Use weak_ptr for dangling-aware shared_ptr
- Item 21: Prefer make_unique and make_shared
- Item 22: Use the Pimpl idiom with unique_ptr
**Time: 45 min**

---

### 6. What Every C++ Developer Should Know About Memory
https://people.freebsd.org/~lstewart/articles/cpumemory.pdf

Ulrich Drepper's classic paper on memory hierarchy (cache, TLB, NUMA). Deep but rewarding. Read sections 2 (CPU caches) and 6 (what programmers can do) at minimum.
**Time: 1-2 hours for selected sections (advanced)**
