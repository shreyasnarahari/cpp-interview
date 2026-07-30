# Topic 16: Error Handling Strategies & std::expected

## Overview
Error handling in C++ spans multiple paradigms, each with performance, safety, and architectural trade-offs:
1. **Exceptions** (`try`/`catch`/`throw`): Zero-cost when non-throwing, high cost when unwinding.
2. **Error Codes** (`std::error_code`, `std::error_condition`): Explicit, deterministic runtime cost.
3. **Monadic Error Types** (`std::expected` in C++23): Functional, zero-allocation, explicit error handling with zero exception unwinding overhead.

---

## Directory Structure
```
16-error-handling-strategies/
├── README.md              ← You are here
├── resources.md           ← Curated reference documents & ISO papers
├── theory-and-mechanics.md← Deep-dive mechanics: zero-cost exceptions vs std::expected monads
└── question-bank/
    ├── real-interview-q-a.md  ← Cold drill questions
    ├── easy.md                ← Basics of exceptions vs return codes
    ├── medium.md              ← std::error_code, custom error categories, system_error
    └── hard.md                ← std::expected, monadic transformations, exception table overhead
```
