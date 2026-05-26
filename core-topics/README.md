# Core Topics

The 15 topics in this folder provide **comprehensive coverage of C++ language interviews** at FAANG, quant firms, and systems companies. Organized by priority — master the high-frequency fundamentals first, then go deeper.

---

## Why These 15?

C++ interviews at companies like Google, Meta, Microsoft, Bloomberg, Amazon, and quant firms consistently test the same core areas. These 15 topics are derived from real interview data, not the full language spec.

---

## Topics

| # | Topic | Priority | Why It's Here |
|---|---|---|---|
| 01 | [Memory Management & RAII](./01-memory-management-raii/) | P1 | The #1 C++ differentiator — smart pointers, ownership, RAII, leaks |
| 02 | [OOP & Polymorphism](./02-oop-polymorphism/) | P1 | Virtual functions, vtable, slicing, diamond problem — asked everywhere |
| 03 | [Templates & Metaprogramming](./03-templates-metaprogramming/) | P1 | SFINAE, specialization, concepts, type traits — senior-level staple |
| 04 | [Move Semantics & Value Categories](./04-move-semantics-value-categories/) | P1 | std::move, perfect forwarding, RVO — modern C++ core |
| 05 | [STL Containers & Iterators](./05-stl-containers-iterators/) | P2 | Iterator invalidation, container choice, complexity — every interview |
| 06 | [Concurrency & Multithreading](./06-concurrency-multithreading/) | P2 | Mutex, atomics, deadlocks, memory model — critical for systems roles |
| 07 | [Lambda Expressions & Functional](./07-lambda-expressions-functional/) | P2 | Captures, mutable, std::function, init captures — modern C++ fluency |
| 08 | [Const Correctness & Type System](./08-const-correctness-type-system/) | P2 | const/constexpr, casting operators, mutable — correctness fundamentals |
| 09 | [Undefined Behavior & Pitfalls](./09-undefined-behavior-pitfalls/) | P2 | UB categories, signed overflow, aliasing, dangling — separates senior from mid |
| 10 | [Exception Handling & noexcept](./10-exception-handling-noexcept/) | P3 | Exception safety guarantees, noexcept + vector, RAII cleanup — Bloomberg favorite |
| 11 | [Design Patterns](./11-design-patterns/) | P3 | Singleton, Factory, CRTP, Pimpl, type erasure — design round staple |
| 12 | [Modern C++ Features](./12-modern-cpp-features/) | P3 | C++17/20/23 features — "What's new?" warm-up question in every interview |
| 13 | [STL Algorithms & Ranges](./13-stl-algorithms-ranges/) | P4 | Algorithm fluency, ranges pipelines, custom comparators — senior-level polish |
| 14 | [Low-Level Memory Layout & Optimization](./14-low-level-memory-layout-optimization/) | P4 | Cache lines, alignment, padding, false sharing — HFT/quant/gaming must-know |
| 15 | [Tooling, Build Systems & Sanitizers](./15-tooling-build-systems-sanitizers/) | P4 | CMake, sanitizers, debuggers, profiling — practical engineering competency |

---

## Priority Guide

| Priority | When to study |
|---|---|
| **P1** — Topics 01–04 | Week 1. These alone cover ~60% of interview questions. |
| **P2** — Topics 05–09 | Week 2. Add another ~20% coverage. |
| **P3** — Topics 10–12 | Week 3. Close remaining gaps — exception safety, patterns, modern standards. |
| **P4** — Topics 13–15 | Week 4. Advanced polish — algorithm fluency, hardware sympathy, tooling. |

---

## How Each Topic Folder Is Structured

```
NN-topic/
├── README.md              ← what's in each file and how to approach
├── resources.md           ← ordered reading list (start here if new)
└── question-bank/
    ├── real-interview-q-a.md  ← questions from real interviews, no answers
    ├── easy.md                ← theory Q&A + coding Q&A with answers
    ├── medium.md              ← theory Q&A + coding Q&A with answers
    └── hard.md                ← theory Q&A + coding Q&A with answers
```

---

## Suggested Study Order

1. Read `resources.md` for the topic
2. Attempt `question-bank/real-interview-q-a.md` cold (no peeking)
3. Check answers in `easy.md` → `medium.md` → `hard.md`
4. Re-implement the coding problems from scratch
