# Topic 15: Tooling, Build Systems & Sanitizers

## Why this topic matters

In a junior C++ interview, the focus is entirely on whether you can write syntactically correct code that solves the problem. 

In a senior C++ interview (especially at companies with massive codebases like Google, Meta, or Bloomberg), interviewers want to know if you can actually **maintain, debug, and build** software in a production environment. Knowing how to write a lock-free queue is useless if you don't know how to integrate it into the build system or how to debug it when it inevitably crashes in production.

## Core Concepts to Master

1. **CMake**: The de facto standard build system generator for C++. You must understand targets, linking, and scope (`PUBLIC`, `PRIVATE`, `INTERFACE`).
2. **Sanitizers**: 
   - **ASan** (AddressSanitizer): For catching use-after-free, buffer overflows, and memory leaks.
   - **TSan** (ThreadSanitizer): For catching data races and deadlocks.
   - **UBSan** (UndefinedBehaviorSanitizer): For catching integer overflow, null pointer dereferences, etc.
3. **Debuggers & Profilers**: Basic fluency in GDB/LLDB and understanding when to use Valgrind vs. ASan.
4. **Compile-Time Optimization**: How to structure headers, use forward declarations, and utilize tools like `ccache` to reduce massive C++ compile times.

## 80/20 Rule for Interviews

If you only have time to master three things here:
1. **ASan & TSan**: Be able to explain exactly what these are and when you would deploy them to solve a "production crash" or "flaky test" scenario.
2. **Modern CMake**: Know the difference between `target_link_libraries` and the legacy `link_libraries`.
3. **Debugging Methodology**: Be able to articulate a step-by-step approach to diagnosing a segfault from a core dump using `gdb backtrace`.
