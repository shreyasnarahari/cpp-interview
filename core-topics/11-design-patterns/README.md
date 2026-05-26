# Design Patterns in Modern C++

## Folder Structure
```
11-design-patterns/
├── README.md              ← you are here
├── resources.md           ← ordered reading list
└── question-bank/
    ├── real-interview-q-a.md   ← real interview questions (no answers)
    ├── easy.md                 ← Singleton, Factory, Observer, Strategy — with answers
    ├── medium.md               ← Pimpl, Builder, Visitor, CRTP patterns — with answers
    └── hard.md                 ← Type erasure, Policy-based design, ECS — with answers
```

## How to Approach

**Day-before review:**
1. Singleton — Meyer's singleton, why it's thread-safe, when NOT to use it
2. Factory Method / Abstract Factory — virtual constructor idiom
3. CRTP — static polymorphism, mixin pattern
4. Pimpl — compilation firewall, unique_ptr with incomplete type
5. Type Erasure — how std::function / std::any work internally
6. Observer — the classic pub/sub, weak_ptr for safe observer lifetime

## Key Insight for Interviews

Interviewers at FAANG companies don't ask "name 23 GoF patterns." They ask:
- "Design a plugin system" → Factory + Strategy
- "How would you decouple this?" → Observer / Pimpl
- "Make this faster without virtual" → CRTP / Policy-based design
- "Store any callable" → Type Erasure

Know the **C++ idiomatic versions** (using templates, smart pointers, RAII), not the Java-style textbook versions.
