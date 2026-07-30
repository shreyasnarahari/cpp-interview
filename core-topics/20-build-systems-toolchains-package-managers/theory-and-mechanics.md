# Theory & Mechanics: Build Systems, Toolchains & Package Managers

## 1. Compiler Optimization Flags Breakdown

```
                    GCC / Clang Optimization Pipeline:
  -O0 ─────────> -O2 ─────────> -O3 ─────────> -flto ─────────> -march=native
(Debug)        (Release)      (Aggressive)    (Link-Time Opt)  (CPU Instruction SIMD)
```

| Flag | Purpose | Mechanics & Impact |
|---|---|---|
| `-O2` | Standard Production | Enables loop vectorization, inlining, register allocation. |
| `-O3` | Aggressive Optimization | Enables loop unrolling, vectorization passes, auto-inlining heuristics. |
| `-flto` / `-ipo` | Link-Time Optimization | Emits LLVM/GIMPLE IR into `.o` files; performs **cross-TU inlining & devirtualization** during link phase. |
| `-march=native` | Target Architecture | Generates SIMD instructions tailored to host CPU (AVX2, AVX-512, FMA). |

---

## 2. Package Management: Conan vs vcpkg

Modern C++ dependency management avoids checking pre-built binaries or third-party code directly into source control:

1. **Conan** (Python-based, Multi-platform): Uses `conanfile.txt` / `conanfile.py` to pull binary packages from ConanCenter. Integration via `find_package()` in CMake.
2. **vcpkg** (Microsoft/Community, C++-native): Manifest mode (`vcpkg.json`). Seamless CMake toolchain file integration (`-DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake`).
