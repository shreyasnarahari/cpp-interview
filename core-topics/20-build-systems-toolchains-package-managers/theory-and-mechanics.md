# Theory & Mechanics: Build Systems & Toolchains

## 1. Modern Target-Based CMake

Avoid global commands (`include_directories`, `add_definitions`). Use target-scoped properties (`PUBLIC`, `PRIVATE`, `INTERFACE`):

- `PRIVATE`: Requirement applies only to the target itself.
- `PUBLIC`: Requirement applies to the target and any downstream targets linking against it.
- `INTERFACE`: Requirement applies only to downstream targets linking against it (header-only libraries).

```cmake
add_library(core_engine ENGINE.cpp)
target_include_directories(core_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(core_engine PRIVATE -Wall -Wextra -Werror)
```

## 2. AddressSanitizer (ASan) Shadow Memory

ASan maps 1/8th of the virtual address space to "shadow bytes" to track byte poison states (redzones, stack frame boundaries, freed memory).
- On access: ASan computes `shadow_addr = (mem_addr >> 3) + offset`.
- If shadow byte is non-zero, an error is reported (`heap-use-after-free`, `buffer-overflow`).
