# Const Correctness & Type System

## Folder Structure
```
08-const-correctness-type-system/
├── README.md              ← you are here
├── resources.md           ← ordered reading list
└── question-bank/
    ├── real-interview-q-a.md   ← real interview questions (no answers)
    ├── easy.md                 ← theory Q&A + coding with answers
    ├── medium.md               ← theory Q&A + coding with answers
    └── hard.md                 ← theory Q&A + coding with answers
```

## How to Approach

**If you're new:**
1. Read `resources.md` (~1 hr) → drill `easy.md`

**If you know basics:**
1. Attempt `real-interview-q-a.md` cold → check answers

**Day-before review:**
1. Read-right-to-left rule for `const` with pointers
2. const member functions and const overloading
3. Four C++ casts — when to use each, and the UB pitfall of `const_cast`
4. `constexpr` vs `const` vs `consteval` vs `constinit`
5. `mutable` — real-world use cases (caching, mutexes)

## What's in Each File

| File | Purpose |
|---|---|
| `resources.md` | Ordered reading list with time estimates |
| `real-interview-q-a.md` | Theory Qs + gotcha code + implementation problems — no answers |
| `easy.md` | const basics, const pointers, const member functions — with answers |
| `medium.md` | mutable, casting operators, constexpr, volatile — with answers |
| `hard.md` | consteval/constinit, const correctness design, strict aliasing — with answers |
