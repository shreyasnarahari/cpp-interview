# Tooling, Build Systems & Sanitizers — Deep Theory & Mechanics

An exhaustive, engineering-grade guide to the C++ compilation pipeline, Link-Time Optimization (LTO), Modern Target-Based CMake, AddressSanitizer (ASan) shadow memory mechanics, ThreadSanitizer (TSan), GDB post-mortem debugging, and Linux `perf` profiling.

---

## 1. The C++ Compilation & Linking Pipeline

```
[ Source File (.cpp) ] -> [ Preprocessor (cpp) ] -> [ Translation Unit ]
                                                          |
                                                          v (Compiler: GCC/Clang)
[ Assembler (as) ] <--- [ Assembly Code (.s) ] <--- [ Optimization / Codegen ]
        |
        v
[ Object File (.o) ] ----+
                         |
[ Static Libs (.a) ] ----+---> [ Linker (ld / mold / lld) ] ---> [ Executable Binary ]
```

### 1.1 Link-Time Optimization (LTO / `-flto`)
- Standard compilation compiles each `.cpp` file in isolation into machine code, preventing the compiler from inlining functions across different translation units.
- **Link-Time Optimization (LTO)** emits GIMPLE/LLVM Intermediate Representation (IR) into object files instead of final machine code. During linking, the whole-program call graph is analyzed, enabling **cross-translation-unit inlining**, dead code elimination, and devirtualization!

---

## 2. AddressSanitizer (ASan) Under the Hood

AddressSanitizer detects out-of-bounds access, use-after-free, and double frees using **Direct Shadow Memory Mapping** and **Redzones**:

```
Virtual Address Space (64-Bit Linux):
+------------------------------------+ 0x7FFFFFFFFFFF
| Application Memory (User Space)    |
+------------------------------------+ 0x10007FFF8000
| Shadow Memory (Tracks State of App)| (Every 8 Bytes of App Memory = 1 Byte Shadow)
+------------------------------------+ 0x00007FFF8000
| Bad Memory Area                    |
+------------------------------------+ 0x000000000000
```

### 2.1 ASan Shadow Memory Math:
$$\text{ShadowAddress} = (\text{AppAddress} \gg 3) + 0\text{x}7\text{FFF}8000$$

- **Redzones**: ASan surrounds heap blocks with poisoned bytes ("redzones"). Any read/write into a redzone triggers an immediate ASan crash report.

---

## 3. Modern Target-Based CMake Architecture

In modern CMake (3.15+), avoid global commands like `include_directories()` and use **Target-Centric Properties**:

```cmake
# 1. Header-Only Interface Target:
add_library(fast_math INTERFACE)
target_include_directories(fast_math INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# 2. Concrete Shared/Static Library:
add_library(core_engine STATIC src/engine.cpp)
target_include_directories(core_engine 
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE src/internal
)
target_link_libraries(core_engine PUBLIC fast_math Threads::Threads)

# 3. Executable:
add_executable(app main.cpp)
target_link_libraries(app PRIVATE core_engine)
```

### Visibility Rules:
- **`PRIVATE`**: Used only by the target itself during compilation.
- **`INTERFACE`**: Provided to consumers of the target, but not used when compiling the target itself.
- **`PUBLIC`**: Used by the target AND propagated to all downstream consumers.

---

## 4. Post-Mortem Crash Debugging with GDB & Core Dumps

```bash
# 1. Enable core dumps on Linux:
ulimit -c unlimited

# 2. Inspect crash in GDB:
gdb ./my_app core.dump

# Useful GDB Commands:
(gdb) bt               # Print full stack backtrace
(gdb) thread apply all bt # Backtrace for ALL threads
(gdb) frame 3          # Switch to stack frame 3
(gdb) info locals      # Print all local variables in frame
(gdb) print *ptr       # Inspect pointer contents
```
