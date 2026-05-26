# Exception Handling & noexcept

## Folder Structure
```
10-exception-handling-noexcept/
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
1. Three exception safety guarantees (no-throw, strong, basic)
2. Why `noexcept` on move operations matters for `std::vector`
3. Catch by const reference, never throw in destructors
4. Copy-and-swap for strong guarantee
5. When exceptions vs error codes (Google style vs Bloomberg style)

## What's in Each File

| File | Purpose |
|---|---|
| `resources.md` | Ordered reading list with time estimates |
| `real-interview-q-a.md` | Theory Qs + gotcha code + implementation problems — no answers |
| `easy.md` | try/catch/throw basics, noexcept, stack unwinding — with answers |
| `medium.md` | Exception safety guarantees, copy-and-swap, RAII + exceptions — with answers |
| `hard.md` | noexcept & move interaction, custom exception hierarchies, exception-safe assignment — with answers |
