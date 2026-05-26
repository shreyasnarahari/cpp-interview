# Tooling, Build Systems & Sanitizers — Hard Questions

---

**Q1. What causes a "False Positive" in ThreadSanitizer (TSan), and how do you resolve it?**

A: TSan monitors memory accesses to detect data races. However, it can sometimes report a false positive if a custom synchronization primitive is used that TSan does not inherently understand (e.g., using inline assembly, hardware transactional memory, or highly unusual atomic patterns).

More commonly, what people think is a "false positive" is actually a real, extremely subtle data race (like an unprotected read of a boolean flag that usually "works" but is technically UB).

If it is a true false positive (e.g., interacting with a legacy 3rd-party binary without TSan instrumentation), you can resolve it by:
1. Using a **Suppression File**: Pass a `.supp` file to the `TSAN_OPTIONS` environment variable instructing TSan to ignore specific functions or threads.
2. Using attributes: Annotate the specific C++ function with `__attribute__((no_sanitize("thread")))`.

---

**Q2. How would you design a Custom Memory Leak Detector without using Valgrind or ASan?**

A: In environments where heavy sanitizers aren't an option (like some embedded platforms or legacy console hardware), you can build a basic leak detector by globally overloading `operator new` and `operator delete`.

1. Overload `void* operator new(size_t size)` to allocate memory and record the pointer address, allocation size, and optionally a stack trace (using OS specific APIs like `backtrace()`) into a thread-safe global hash map.
2. Overload `void operator delete(void* ptr)` to remove the pointer from the hash map.
3. At program termination (or at specific checkpoints), iterate through the hash map and report any pointers that remain, as these represent memory leaks.

---

**Q3. Explain `INTERFACE` targets in CMake. When and why would you use them?**

A: In CMake, targets are usually binaries (`add_executable`) or libraries (`add_library`). An `INTERFACE` library is a target that has **no build output** (no `.a`, `.so`, or `.dll`).

**When to use it**:
1. **Header-Only Libraries**: If you write a library entirely in `.h` or `.hpp` files (like many template libraries), there is nothing to compile. You create an `INTERFACE` library, attach the include directories to it via `target_include_directories(my_lib INTERFACE include/)`, and consumers just link to it.
2. **Compiler Flags**: You can create an interface target just to group compiler warnings or features. 
   ```cmake
   add_library(StrictWarnings INTERFACE)
   target_compile_options(StrictWarnings INTERFACE -Wall -Wextra -Werror)
   
   # Now any target can inherit these warnings instantly
   target_link_libraries(App PRIVATE StrictWarnings)
   ```

---

**Q4. What is `ccache` and how does it speed up C++ builds?**

A: `ccache` (Compiler Cache) acts as a wrapper around your C++ compiler (like GCC or Clang). 

When you compile a `.cpp` file, `ccache` hashes the preprocessed source code (the `.cpp` file + all included headers after macros are expanded) along with the compiler flags. 
- If that exact hash exists in the cache, `ccache` skips compilation entirely and serves the pre-compiled `.o` object file directly from its cache directory.
- If it's a miss, it compiles normally and caches the result.

This makes switching branches, rebasing, or cleaning a build extremely fast, as unmodified translation units are served instantly.

---

## Gotcha Code

```cpp
// Q: You are debugging a massive codebase. A crash happens intermittently in production.
// You compile with ASan (-fsanitize=address), but the program now crashes immediately 
// on startup, completely unrelated to the production crash! What is happening?

// A: One Definition Rule (ODR) Violation!
// ASan inherently checks for ODR violations (e.g., two different translation units 
// defining a global variable or struct with the same name but different sizes).
// In a normal build, the linker might silently pick one, leading to silent memory corruption.
// ASan aggressively halts the program if it detects this. 
// You must fix the ODR violation (usually by putting internal globals into anonymous namespaces) 
// before you can proceed with debugging the original crash.
```
