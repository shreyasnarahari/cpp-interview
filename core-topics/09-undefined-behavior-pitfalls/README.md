# Undefined Behavior & Common Pitfalls

## Folder Structure
```
09-undefined-behavior-pitfalls/
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
1. The Big 5 UB categories: signed overflow, null deref, out-of-bounds, use-after-free, data races
2. The difference between undefined, unspecified, and implementation-defined behavior
3. Sanitizer flags: `-fsanitize=address,undefined,thread`
4. "The compiler can assume UB never happens" — time-travel optimization
5. Common gotchas: dangling references, iterator invalidation, signed/unsigned comparison

## What's in Each File

| File | Purpose |
|---|---|
| `resources.md` | Ordered reading list with time estimates |
| `real-interview-q-a.md` | Theory Qs + "What does this print?" gotchas — no answers |
| `easy.md` | Basic UB categories, common traps, sanitizer usage — with answers |
| `medium.md` | Order of evaluation, strict aliasing, sequence points — with answers |
| `hard.md` | Compiler UB exploitation, memory model violations, ABI pitfalls — with answers |
