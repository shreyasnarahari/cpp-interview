# Tooling, Build Systems & Sanitizers — Easy Questions

---

**Q1. What is CMake? Why use it instead of Make?**

A: CMake is a **meta-build system**. It does not build code directly. Instead, it generates the native build files for your specific platform (e.g., Makefiles on Linux, Ninja build files, or Visual Studio solutions on Windows).

**Why use it?**
- **Cross-Platform**: A single `CMakeLists.txt` works across Windows, macOS, and Linux.
- **Dependency Management**: It makes it easier to find and link external libraries (`find_package`).
- **Out-of-source builds**: CMake strongly encourages building in a separate `build/` directory, keeping your source tree clean.

---

**Q2. What are AddressSanitizer (ASan) and ThreadSanitizer (TSan)?**

A: They are compiler instruments (flags) developed by Google and integrated into Clang and GCC to detect bugs at runtime.

- **ASan** (`-fsanitize=address`): Detects memory corruption bugs like use-after-free, heap/stack buffer overflows, double-free, and memory leaks.
- **TSan** (`-fsanitize=thread`): Detects data races (when two threads access the same memory concurrently without synchronization, and at least one is a write) and deadlocks.

*Gotcha*: You **cannot** use ASan and TSan at the same time in a single binary. You must compile and run your test suite twice (once with ASan, once with TSan).

---

**Q3. What is the difference between a compile-time error, a link-time error, and a run-time error?**

A:
1. **Compile-time**: The compiler fails to translate a single `.cpp` file into an object (`.o`) file. Example: Syntax errors, missing `#include`, type mismatches.
2. **Link-time**: The compiler succeeded, but the linker fails to combine the `.o` files into an executable. Example: `undefined reference to functionX` (you declared a function but forgot to implement it, or forgot to link the library containing it).
3. **Run-time**: The executable builds successfully but crashes or behaves incorrectly when executed. Example: Segmentation fault, data race, logic error.

---

**Q4. What are the basic GDB commands you use to debug a crash?**

A: If an application crashes and generates a core dump, you load it into GDB (`gdb ./my_app core`).
1. `bt` (backtrace): Shows the call stack leading up to the crash.
2. `frame N`: Switches to a specific stack frame `N` from the backtrace.
3. `print var`: Prints the value of a local variable or pointer.
4. `info locals`: Prints all local variables in the current frame.
5. `up` / `down`: Moves up or down the call stack to inspect caller variables.

---

## Gotcha Code

```cpp
// Q: You compile this code and get an "Undefined Reference" linker error. 
// However, the function IS defined in math_utils.cpp. What went wrong?

// main.cpp
#include "math_utils.h"
int main() {
    int result = add(5, 10);
}

// A: The build system (e.g., CMake or raw g++ command) was not told to 
// compile and link `math_utils.cpp`.
// WRONG: g++ main.cpp -o app
// CORRECT: g++ main.cpp math_utils.cpp -o app
```
