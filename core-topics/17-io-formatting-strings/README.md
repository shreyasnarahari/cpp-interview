# Topic 17: I/O, Formatting & Strings

## Overview
String processing and I/O performance are fundamental to production systems:
1. **Small String Optimization (SSO)**: In-line stack storage for short strings avoiding heap allocations.
2. **Zero-Copy String Views** (`std::string_view`): Non-owning references to string data.
3. **Modern Formatting** (`std::format` in C++20, `std::print` in C++23): Type-safe, high-performance replacements for `printf` and `iostream`.

---

## Directory Structure
```
17-io-formatting-strings/
├── README.md              ← You are here
├── resources.md           ← Reference guides & standard proposals
├── theory-and-mechanics.md← Internal mechanics of SSO, memory layout, and formatting ASTs
└── question-bank/
    ├── real-interview-q-a.md  ← Cold drill question set
    ├── easy.md                ← SSO thresholds, basic string_view usage
    ├── medium.md              ← std::format vs iostream performance, string_view lifetime traps
    └── hard.md                ← Custom std::formatter implementation, custom fast float parsing
```
