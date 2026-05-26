# Concurrency & Multithreading — Learning Resources

---

### 1. C++ Concurrency in Action (2nd Ed) — Anthony Williams
Book — THE reference for C++ concurrency. Read chapters 1–5 for interview prep.
**Time: 2–3 hours for key chapters**

---

### 2. Back to Basics: Concurrency — CppCon (Arthur O'Dwyer)
https://www.youtube.com/watch?v=F6Ipn7gCOsY

Covers threads, mutexes, condition variables, and atomics. Clear, practical examples.
**Time: 60 min (watch at 1.5x → 40 min)**

---

### 3. cppreference — Thread Support Library
https://en.cppreference.com/w/cpp/thread

Complete API reference for `std::thread`, `std::mutex`, `std::atomic`, `std::condition_variable`, `std::future`.
**Time: 20 min (skim)**

---

### 4. Effective Modern C++ — Items 35–40 (Scott Meyers)
Key items: prefer task-based (async) to thread-based programming, use `std::atomic` for special memory, make `std::thread`s unjoinable on all paths.
**Time: 30 min**

---

### 5. C++ Memory Model — Herb Sutter
https://herbsutter.com/2013/02/11/atomic-weapons-the-c-memory-model-and-modern-hardware/

Deep dive into the memory model, atomic operations, and memory orderings. Essential for senior/quant roles.
**Time: 60 min**
