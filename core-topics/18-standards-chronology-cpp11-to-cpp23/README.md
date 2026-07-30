# Topic 18: Standards Chronology (C++11 to C++23)

## Overview
Understanding the evolution of the ISO C++ standard allows developers to modernize legacy codebases and write expressive, zero-cost modern abstractions:
1. **C++11**: The Modern C++ Revolution (Move semantics, smart pointers, lambdas, auto, constexpr, threads).
2. **C++14**: Refinements (Generic lambdas, `std::make_unique`, relaxed constexpr).
3. **C++17**: Ergonomics & Vocabulary Types (`std::optional`, `std::variant`, `std::string_view`, structured bindings, `if constexpr`).
4. **C++20**: Language Paradigm Shift (Concepts, Ranges, Coroutines, Modules, `std::format`, `<numbers>`).
5. **C++23**: Monadic Utilities & Explicit Object Parameters (`std::expected`, `std::print`, `deducing this`, `std::flat_map`).

---

## Directory Structure
```
18-standards-chronology-cpp11-to-cpp23/
├── README.md              ← You are here
├── resources.md           ← ISO standard timeline & compiler support matrices
├── theory-and-mechanics.md← Standard evolution breakdown, breaking changes & deprecations
└── question-bank/
    ├── real-interview-q-a.md  ← Cold drill question bank
    ├── easy.md                ← Features by standard Q&A
    ├── medium.md              ← Migration of C++03/11 code to C++20/23
    └── hard.md                ← In-depth feature interaction (e.g. Deducing This vs CRTP, Modules ABI)
```
