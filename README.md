# C++ Language Mastery & Engineering Reference

An exhaustive, deep-dive reference repository and engineering laboratory for mastering modern C++ (C++11 through C++23), low-level memory mechanics, template metaprogramming, low-latency performance optimization, and production system design.

---

## Directory Architecture

```
cpp-interview/
├── syllabus.txt          ← Comprehensive 20-topic C++ specification reference blueprint
├── real-interview-q/     ← Company-specific interview question files (Google, Meta, etc.)
├── citadel-prep.md       ← HFT / Quant low-latency C++ engineering guide & lock-free queues
├── core-topics/          ← 20 exhaustive topic modules with deep-dive theory & code banks
│   ├── 01-memory-management-raii/
│   ├── 02-oop-polymorphism/
│   ├── 03-templates-metaprogramming/
│   ├── 04-move-semantics-value-categories/
│   ├── 05-stl-containers-iterators/
│   ├── 06-concurrency-multithreading/
│   ├── 07-lambda-expressions-functional/
│   ├── 08-const-correctness-type-system/
│   ├── 09-undefined-behavior-pitfalls/
│   ├── 10-exception-handling-noexcept/
│   ├── 11-design-patterns/
│   ├── 12-modern-cpp-features/
│   ├── 13-stl-algorithms-ranges/
│   ├── 14-low-level-memory-layout-optimization/
│   ├── 15-tooling-build-systems-sanitizers/
│   ├── 16-error-handling-strategies/
│   ├── 17-io-formatting-strings/
│   ├── 18-standards-chronology-cpp11-to-cpp23/
│   ├── 19-cpp-core-guidelines-idioms/
│   └── 20-build-systems-toolchains-package-managers/
├── system-level-programming/ ← Low-latency, Linux kernel primitives & hardware engineering lab
└── practice-cpp/         ← Executable C++20/23 CMake laboratory & performance benchmarks
```

---

## Core Components

### 1. `syllabus.txt`
The master C++ map — 20 detailed modules covering everything from preprocessor translation units and memory layouts to template concepts, lock-free concurrency, and C++23 standard library additions (`std::expected`, `std::print`, `deducing this`).

### 2. `core-topics/`
The primary mastery curriculum containing 20 dedicated topic modules. Each module provides:
- **Exhaustive Deep-Dive Theory & Mechanics**: In-depth explanations of language semantics, compiler assembly generation, and standard library internals.
- **Curated Learning Resources**: Hand-picked reading materials, standard specification references, and papers.
- **Graded Problem Banks**: Complete Question & Answer banks covering Easy, Medium, Hard, and Real-World engineering scenarios.

See [core-topics/README.md](./core-topics/README.md) for the full 20-module syllabus roadmap.

### 3. `system-level-programming/`
Advanced low-latency systems and kernel engineering laboratory covering:
- Nanosecond hardware cycle timing (`rdtsc`/`rdtscp`) and latency distribution tracking.
- Zero-allocation lock-free diagnostic & async-signal-safe logging.
- Cache-line alignment, hardware interference sizes, and struct memory layout inspection.
- 12-module curriculum bridging bare-metal hardware, Linux systems programming, database kernels (storage, indexing, MVCC/recovery), and production capstone systems.

See [system-level-programming/README.md](./system-level-programming/README.md) for the complete roadmap.

### 4. `real-interview-q/`
Company-specific interview question files split by firm: `google.md`, `meta.md`, `bloomberg.md`, `nokia.md`, `adobe.md`, `amazon.md`, `microsoft.md`, `jane-street.md`, and `citadel.md`. Each contains tagged questions by topic (Beginner/Intermediate/Advanced), gotcha code blocks, company-specific focus areas, and the master gotcha table.

### 5. `citadel-prep.md`
Low-latency and performance-critical engineering guide covering lock-free data structures (SPSC ring buffers with `alignas(64)` cache isolation), custom arena allocators, cache hierarchy mechanics, and zero-allocation hot paths.

### 6. `practice-cpp/`
A CMake-backed technical laboratory for compiling C++ snippets, running address/thread sanitizers, and conducting micro-benchmarks (e.g. dynamic vtable dispatch vs. static CRTP polymorphism).

---

## Master Recommended Study & Engineering Workflow

1. **Exhaustive Theory**: Read through the targeted module in `core-topics/` (01 through 20).
2. **Deep Mechanics**: Analyze assembly outputs, vtable layouts, and memory representations.
3. **Problem Drilling**: Solve the problem banks (`easy.md` → `medium.md` → `hard.md`) and verify against production-grade C++ code implementations.
4. **Hands-on Experimentation**: Test, benchmark, and run sanitizers inside `practice-cpp/`.

