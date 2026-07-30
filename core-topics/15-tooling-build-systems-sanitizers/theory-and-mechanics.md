# Theory & Mechanics: Tooling, Build Systems & Sanitizers

## 1. Modern Target-Based CMake Architecture

Modern CMake (3.x+) uses a **Target-Centric Model**. Avoid global directory properties (`include_directories`, `add_definitions`). Treat targets (`add_library`, `add_executable`) as objects with scoped properties:

```cmake
add_library(engine_core STATIC src/engine.cpp)

# Target-Scoped Properties:
target_include_directories(engine_core PUBLIC include)
target_compile_options(engine_core PRIVATE -Wall -Wextra -Werror)
```

### Visibility Rules: `PUBLIC`, `PRIVATE`, `INTERFACE`

| Visibility | Applies to Target itself? | Transmitted to Downstream Dependents? | Use Case |
|---|---|---|---|
| **`PRIVATE`** | **YES** | No | Internal implementation headers (`src/internal.h`) |
| **`PUBLIC`** | **YES** | **YES** | Public API headers required by callers (`include/engine.h`) |
| **`INTERFACE`** | No | **YES** | Header-only libraries (Templates, header interfaces) |

---

## 2. Compiler Sanitizers & Shadow Memory Architecture

Sanitizers insert runtime instrumentation instructions during compilation to catch bugs at near-native speed (1.5x–3x overhead vs 20x–100x Valgrind overhead).

```
                      Sanitizer Shadow Memory Allocation (ASan):
  Virtual Address Space (64-bit):
  ┌───────────────────────────────┬───────────────────────────────┐
  │ Application Memory (8 Bytes)  │  Shadow Memory Index (1 Byte) │
  └───────────────────────────────┴───────────────────────────────┘
                                                  ▲
  shadow_addr = (app_addr >> 3) + 0x7fff8000 ─────┘
```

### 1. AddressSanitizer (ASan / `-fsanitize=address`)
- **How it works**: Allocates 1/8th of virtual memory as **Shadow Memory**. Every 8 bytes of application memory maps to 1 byte of shadow memory storing poison state.
- **Redzones**: ASan places poisoned "redzone" memory blocks around every stack/heap allocation.
- **Detected Faults**: Out-of-bounds array access, Use-After-Free, Double Free, Memory Leaks (LSan).

### 2. ThreadSanitizer (TSan / `-fsanitize=thread`)
- **How it works**: Instruments every memory read and write instruction to maintain shadow state and vector clocks for every 8-byte memory block.
- **Detected Faults**: Unsynchronized concurrent Data Races across threads.

### 3. UndefinedBehaviorSanitizer (UBSan / `-fsanitize=undefined`)
- **How it works**: Inserts assertion instructions before potentially dangerous operations.
- **Detected Faults**: Signed integer overflow, misaligned pointer accesses, bit-shifts beyond bit-width, null pointer dereferences.

---

## 3. GDB / LLDB Core Dump Debugging

When a production C++ microservice crashes with a Segmentation Fault, the OS dumps the process memory state into a **core dump file**:

```bash
# Load core dump into GDB with debug symbols (-g)
gdb ./bin_target /var/dumps/core.4892
```

### Essential GDB Debugging Commands:
- `bt` / `backtrace`: Prints call stack trace leading to the crash point.
- `frame N`: Switches context to stack frame `N`.
- `info locals`: Prints all local variables inside the current frame.
- `print *ptr`: Evaluates and prints object structure pointed to by `ptr`.
- `thread apply all bt`: Prints stack traces for ALL running threads simultaneously (essential for deadlocks).

---

## 4. Build Performance & Linker Optimization

In large C++ codebases, incremental builds are slow due to compile-time dependency cascades and slow linking:

1. **Fast Modern Linkers**: Replace default `ld` with **`lld`** (LLVM Linker) or **`mold`** (High-speed multi-threaded linker, 10x–50x faster linking).
2. **Compiler Caching (`ccache`)**: Caches intermediate `.o` object files across builds; skips compilation when header hashes match.
3. **Compilation Firewalls**: Use forward declarations instead of `#include` in headers, and use the **Pimpl Idiom** to isolate implementation details.
