# Move Semantics & Value Categories

## Folder Structure
```
04-move-semantics-value-categories/
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
1. Read `resources.md` (~1.5 hrs) → drill `easy.md`

**If you know basics:**
1. Attempt `real-interview-q-a.md` cold → check answers in easy/medium/hard

**Day-before review:**
1. "std::move is just a cast" — know it cold
2. RVO vs NRVO — when is copy elision guaranteed?
3. Forwarding references vs rvalue references — the `T&&` dual meaning
4. noexcept on move operations — why it matters for std::vector

## What's in Each File

| File | Purpose |
|---|---|
| `resources.md` | Ordered reading list with time estimates |
| `real-interview-q-a.md` | Theory Qs + gotcha code + implementation problems — no answers |
| `easy.md` | lvalue/rvalue basics, move constructor, std::move, RVO — with answers |
| `medium.md` | Forwarding references, reference collapsing, perfect forwarding, guaranteed copy elision — with answers |
| `hard.md` | Value categories (5 kinds), exception safety interaction, RVO mechanism, std::move implementation — with answers |
