# Tooling, Build Systems & Sanitizers — Real Interview Questions

---

**Q1. Debugging a Production Crash [Meta]**

*Interviewer:* "One of our backend C++ microservices crashes intermittently in production every few days. The core dump points to a segmentation fault deep inside `std::string` operations, but the string looks like complete garbage memory. How do you find the root cause?"

A: 
This strongly points to a memory corruption bug, likely a **Use-After-Free** or a **Heap Buffer Overflow**. The crash location in `std::string` is just a victim of memory that was corrupted elsewhere.

**Action Plan:**
1. **Enable ASan in Staging/Canary**: Recompile the microservice with `-fsanitize=address` and deploy it to a canary cluster receiving a subset of production traffic.
2. **Reproduce the Bug**: Because ASan poisons freed memory and surrounding redzones, the *moment* the offending code writes or reads out of bounds, ASan will crash the program with a deterministic, descriptive stack trace pointing exactly to the source line of the memory violation, rather than the innocent `std::string` operation that failed later.
3. **If ASan isn't feasible in prod**: Ensure the binary is compiled with debug symbols (`-g`). Load the core dump in `gdb`. Use `bt` (backtrace) to identify the caller. If the data is garbage, check if the object's pointer was captured by a lambda running on a detached thread where the lifetime of the object expired.

---

**Q2. The Flaky Test [Google]**

*Interviewer:* "We have a unit test for our new concurrent Queue. It passes 99% of the time in CI, but sometimes fails randomly. How do you approach fixing this?"

A: 
A flaky test in concurrent code is almost always a **Data Race** or an unprotected **Memory Ordering** issue. 

**Action Plan:**
1. **ThreadSanitizer (TSan)**: Recompile the test suite with `-fsanitize=thread` and run it locally. Often, TSan will catch a data race even if the test technically passes, because it monitors the memory access patterns regardless of the logical outcome.
2. **Stress Testing**: If TSan doesn't catch it on a single run, put the test in a bash loop and run it 10,000 times under TSan.
3. **Analyze the Output**: TSan will print a warning showing "Thread T1 wrote to memory X at this line... Previous read by Thread T2 was at this line."
4. **Fix**: Ensure proper `std::mutex` locks protect the shared state, or if using `std::atomic`, verify that the memory orderings (e.g., using `relaxed` when `acquire/release` was necessary) are correct.

---

**Q3. Build System Design for Massive Repositories [Bloomberg]**

*Interviewer:* "Our monolith takes 2 hours to compile from scratch, and even changing a single `.cpp` file takes 5 minutes to incrementally build. As a Staff Engineer, what architectural and tooling steps do you take to fix this?"

A: 
This is a classic C++ compile-time dependency problem.

1. **Tooling Solutions**:
   - Install **`ccache`** to cache object files.
   - Use a distributed build system like **distcc** or **Goma** (to spread compilation across many machines).
   - Switch the linker from standard `ld` to **`lld`** (LLVM's linker) or **`mold`**, which are exponentially faster at linking massive binaries.

2. **Architectural / CMake Solutions**:
   - **Forward Declarations**: Remove `#include` directives from header files wherever possible. Use pointers/references and forward-declare the classes. Move the actual `#include` into the `.cpp` file.
   - **Pimpl Idiom**: Hide heavy implementation details behind a pointer (Pointer to Implementation). This acts as a compilation firewall.
   - **CMake `PRIVATE` linking**: Ensure `target_link_libraries` uses `PRIVATE` as much as possible. If Library A depends on Library B via `PUBLIC`, any change to B forces a recompile of A *and everything that depends on A*. `PRIVATE` stops the compilation cascade.
   - **Split Targets**: Break the monolith down into smaller, static libraries or shared libraries using CMake.
