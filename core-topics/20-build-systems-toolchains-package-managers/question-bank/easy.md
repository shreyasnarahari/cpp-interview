# Foundational Questions: CMake & Compiler Flags

### Q1: What compiler flag enables AddressSanitizer in GCC/Clang?
**Answer:** `-fsanitize=address -fno-omit-frame-pointer -g`.

### Q2: What is `target_link_libraries` in CMake?
**Answer:** It specifies libraries or targets that a given CMake target depends on, propagating compile options and include directories based on visibility rules (`PUBLIC`, `PRIVATE`, `INTERFACE`).
