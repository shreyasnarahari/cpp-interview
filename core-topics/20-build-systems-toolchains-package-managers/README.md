# Topic 20: Build Systems, Toolchains & Package Managers

## Overview
Modern C++ engineering relies on production build tools, static analysis, sanitizers, and package managers:
1. **Modern CMake**: Target-centric design (`target_link_libraries`, `target_include_directories`, interface targets).
2. **Compiler Optimization Flags**: `-O3`, `-flto` (Link Time Optimization), `-march=native`, `-fno-rtti`, `-fno-exceptions`.
3. **Runtime Sanitizers**: AddressSanitizer (ASan), ThreadSanitizer (TSan), UndefinedBehaviorSanitizer (UBSan).
4. **Package Management**: Conan and vcpkg dependency management integration.

---

## Directory Structure
```
20-build-systems-toolchains-package-managers/
├── README.md              ← You are here
├── resources.md           ← Modern CMake documentation & sanitizer guides
├── theory-and-mechanics.md← Target-based CMake mechanics, LTO, sanitizer memory shadows
└── question-bank/
    ├── real-interview-q-a.md  ← Cold drill question set
    ├── easy.md                ← CMake basics, compiler flags
    ├── medium.md              ← Sanitizer usage, static analysis integration
    └── hard.md                ← LTO/IPO build pipelines, cross-compilation & package manager integration
```
