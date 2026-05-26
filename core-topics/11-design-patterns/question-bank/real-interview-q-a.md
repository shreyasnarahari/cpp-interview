# Design Patterns — Real Interview Questions

> No answers — attempt cold first.

## Theory Questions

1. What is the Singleton pattern? Implement a thread-safe version in C++.
2. What is the Factory Method pattern? How does it differ from Abstract Factory?
3. What is the Strategy pattern? Implement it with both virtual functions and templates.
4. What is the Observer pattern? How do you handle observer lifetime safely?
5. What is the Pimpl idiom? Why is it a pattern and not just a trick?
6. What is CRTP? Give 3 real-world use cases.
7. What is type erasure? How is `std::function` implemented?
8. What is the Builder pattern? When is it useful in C++?
9. What is the Visitor pattern? What is the "double dispatch" problem?
10. What is policy-based design? How does it differ from strategy?
11. What is the Decorator pattern? How do you implement it in C++?
12. What is the difference between compile-time and runtime polymorphism patterns?

## Implementation Problems

1. Design a logger with pluggable output strategies (console, file, network).
2. Implement a command pattern with undo/redo support.
3. Write a type-erased `Any` class (simplified `std::any`).
4. Design a plugin system using Factory + shared libraries.
5. Implement a thread-safe event bus using Observer pattern.
