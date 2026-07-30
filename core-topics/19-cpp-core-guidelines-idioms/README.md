# Topic 19: C++ Core Guidelines & Production Idioms

## Overview
Writing production-grade C++ requires adhering to industry-proven guidelines and architectural design idioms:
1. **C++ Core Guidelines** (Bjarne Stroustrup & Herb Sutter): Resource management rules, lifetime safety, type safety, interface rules.
2. **Key Idioms**:
   - **RAII**: Resource Acquisition Is Initialization.
   - **Pimpl Idiom**: Pointer to Implementation (ABI stability & compilation firewall).
   - **Type Erasure**: Polymorphism without common base class inheritance (`std::function`, `std::any`).
   - **Policy-Based Design**: Template-driven behavioral composition.

---

## Directory Structure
```
19-cpp-core-guidelines-idioms/
├── README.md              ← You are here
├── resources.md           ← C++ Core Guidelines repository & papers
├── theory-and-mechanics.md← Architectural mechanics of Pimpl, Type Erasure, and Core Guidelines rules
└── question-bank/
    ├── real-interview-q-a.md  ← Cold drill question set
    ├── easy.md                ← Rule of Zero/Three/Five, RAII basics
    ├── medium.md              ← Pimpl pattern implementation & ABI firewalls
    └── hard.md                ← Type erasure implementation from scratch (vtables, memory layout)
```
