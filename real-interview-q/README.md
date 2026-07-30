# Real Interview Questions — By Company

C++ interview questions organized by company, extracted from real interviews. Each file contains:
- **Tagged questions by topic** — questions with difficulty levels (Beginner/Intermediate/Advanced) and gotcha code blocks
- **Company-specific focus questions** — the areas each company emphasizes
- **Master gotcha table** — quick reference for common C++ interview traps

---

## Files

| Company | File | Tagged Questions | Focus Area |
|---|---|---|---|
| Google | [google.md](./google.md) | 87 | Language mechanics & design |
| Jane Street / Two Sigma | [jane-street.md](./jane-street.md) | 42 | Low-level, performance, templates |
| Meta (Facebook) | [meta.md](./meta.md) | 38 | Concurrency & production systems |
| Bloomberg | [bloomberg.md](./bloomberg.md) | 34 | RAII, const correctness, OOP |
| Amazon | [amazon.md](./amazon.md) | 17 | Practical coding & correctness |
| Microsoft | [microsoft.md](./microsoft.md) | 6 | OOP & systems |
| Citadel | [citadel.md](./citadel.md) | 24 (focus) | Low-latency, lock-free, hardware sympathy |

> **Note:** Citadel's questions are primarily in the [Company-Specific Focus section](./citadel.md) and the dedicated [citadel-prep.md](../citadel-prep.md) guide, rather than inline topic tags.

---

## 80/20 Priority Guide

| Priority | Topics | Coverage |
|---|---|---|
| P1 — Do first | Memory Management & RAII, OOP & Polymorphism, Templates, Move Semantics | ~60% of all C++ interviews |
| P2 — Week 2 | STL Containers, Concurrency, Lambdas, Const Correctness, UB & Pitfalls | ~20% additional |
| P3 — Senior level | Memory model, Template metaprogramming, Cache optimization, Design patterns, Coroutines | Remaining 20% |
