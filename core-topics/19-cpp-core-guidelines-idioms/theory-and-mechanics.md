# Theory & Mechanics: C++ Core Guidelines & Production Idioms

## 1. C++ Core Guidelines Summary (Bjarne Stroustrup & Herb Sutter)

The C++ Core Guidelines provide production-proven rules for writing type-safe, resource-safe, and high-performance modern C++:

### Key Guideline Categories:

1. **Resource Management (R.1 – R.37)**:
   - **R.1**: Never manage resources manually using raw `new` and `delete`. Use RAII wrappers (`std::unique_ptr`, `std::shared_ptr`, `std::vector`).
   - **R.10**: Avoid raw pointers (`T*`) for ownership transfer. Raw pointers denote **non-owning observers**.

2. **Interface Design (I.1 – I.30)**:
   - **I.2**: Avoid global variables (eliminates static initialization race conditions).
   - **I.11**: Never pass a raw pointer for a non-nullable parameter. Use references `T&`.
   - **I.23**: Keep number of function parameters minimal (group related parameters into structs).

3. **Functions & Expressions (F.1 – F.60)**:
   - **F.16**: For input parameters, pass cheap-to-copy types by value, large types by `const T&`, and move-only types by value (`std::move`).

---

## 2. Fundamental C++ Production Idioms

```
                  Essential Modern C++ Production Idioms:
  ┌───────────────┬───────────────────────────────┬───────────────────────────────┐
  │     RAII      │         Pimpl Idiom           │         Type Erasure          │
  ├───────────────┼───────────────────────────────┼───────────────────────────────┤
  │ Resource      │ Pointer to Implementation.    │ Polymorphism without base     │
  │ scope-bound   │ Compilation firewall,         │ inheritance wrappers          │
  │ cleanup.      │ ABI binary stability.         │ (std::function, std::any).    │
  └───────────────┴───────────────────────────────┴───────────────────────────────┘
```
