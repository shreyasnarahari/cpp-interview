# Tooling, Build Systems & Sanitizers — Medium Questions

---

**Q1. How does AddressSanitizer (ASan) work under the hood?**

A: ASan works by using **Shadow Memory** and **Poisoning**.
When you compile with `-fsanitize=address`, the compiler allocates a dedicated "shadow" region of memory where every 1 byte of shadow memory tracks the state of 8 bytes of application memory.

Whenever you allocate memory (e.g., via `new` or `malloc`), ASan surrounds the allocated block with "Redzones" (poisoned memory). It also injects a tiny check before *every single memory read or write* in your code.
If your code tries to read/write to a poisoned Redzone (a buffer overflow), or accesses memory that was previously freed and marked as poisoned (use-after-free), the injected check fails, and ASan immediately halts the program and prints a detailed stack trace.

Because the checks are heavily optimized by the compiler, the overhead is only about ~2x slowdown, making it perfectly usable in CI/CD pipelines.

---

**Q2. Explain the difference between `target_include_directories` and `include_directories` in modern CMake.**

A: This is the core of **Modern Target-Based CMake**.

- `include_directories()` is a **legacy, global** command. It adds the include path to *every* target defined in the current `CMakeLists.txt` and all subdirectories. This leads to namespace pollution, slow compile times, and spaghetti dependencies because targets inherit paths they don't actually need.
- `target_include_directories(my_target ...)` is the **modern** approach. It strictly associates the include path with a specific target (`my_target`). This encapsulates the dependency, ensuring that only the target that needs the header gets the path.

---

**Q3. What is UndefinedBehaviorSanitizer (UBSan) and what does it catch?**

A: Compiled with `-fsanitize=undefined`, UBSan catches operations that invoke Undefined Behavior in C++ at runtime.
Unlike ASan, which focuses strictly on memory layout violations, UBSan catches logical language violations:
- Signed integer overflow
- Division by zero
- Dereferencing a `nullptr`
- Misaligned pointers
- Shifting a bit past the size of an integer
- Returning from a non-void function without a `return` statement

It has extremely low overhead and is often run alongside ASan.

---

**Q4. What is the difference between Valgrind (Memcheck) and AddressSanitizer?**

A: 
- **Compilation**: ASan requires you to recompile your source code with a special flag. Valgrind is a dynamic binary instrumentation framework; it runs on existing, unmodified binaries.
- **Performance**: ASan slows down the application by roughly 2x. Valgrind emulates the CPU and slows down the application by 10x to 50x.
- **Accuracy**: ASan can catch stack buffer overflows and global variable overflows because the compiler knows about them and poisons them. Valgrind generally only catches heap-based errors because it intercepts `malloc` and `free` at runtime.

*Conclusion*: Always prefer ASan during development and CI. Use Valgrind only if you cannot recompile the binary or if you need specific profiling tools (like Callgrind).

---

## Gotcha Code

```cpp
// Q: In CMake, what is the difference between these two target associations?
target_link_libraries(App PRIVATE LibA)
target_link_libraries(App PUBLIC LibB)

// A: Usage Requirements and Transitive Dependencies.
// PRIVATE means 'App' uses 'LibA' internally. If another target 'SuperApp' 
// links to 'App', it will NOT inherit 'LibA'.
// PUBLIC means 'App' uses 'LibB' in its public API (e.g., includes a LibB header 
// inside App's own header file). If 'SuperApp' links to 'App', it automatically 
// inherits 'LibB's include directories and links against it too.
```
