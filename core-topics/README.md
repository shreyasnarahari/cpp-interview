# C++ Mastery Modules (01–20)

Exhaustive technical coverage of the C++ programming language, standard library, software architecture, memory mechanics, and performance engineering.

---

## Master 20-Module Curriculum

| Module | Topic | Core Focus & Engineering Scope |
|---|---|---|
| 01 | [Memory Management & RAII](./01-memory-management-raii/) | Smart pointers (`unique_ptr`, `shared_ptr`, `weak_ptr`), ownership semantics, RAII, custom deleters, stack vs heap mechanics |
| 02 | [OOP & Polymorphism](./02-oop-polymorphism/) | Virtual dispatch, vtable/vptr layout, dynamic vs static polymorphism, object slicing, diamond problem, virtual destructors |
| 03 | [Templates & Metaprogramming](./03-templates-metaprogramming/) | Template deduction, specialization, SFINAE (`std::enable_if`, `void_t`), Concepts (C++20), variadic templates, fold expressions |
| 04 | [Move Semantics & Value Categories](./04-move-semantics-value-categories/) | lvalues, rvalues, prvalues, xvalues, `std::move`, perfect forwarding (`std::forward`), RVO/NRVO, reference collapsing |
| 05 | [STL Containers & Iterators](./05-stl-containers-iterators/) | Container internal structures (`vector`, `deque`, `list`, `map`, `unordered_map`), iterator invalidation rules, complexity guarantees, `std::span` |
| 06 | [Concurrency & Multithreading](./06-concurrency-multithreading/) | Mutexes, `std::atomic`, memory orderings (relaxed, acquire/release, seq_cst), lock-free data structures, condition variables, `jthread` |
| 07 | [Lambda Expressions & Functional](./07-lambda-expressions-functional/) | Captures (`*this`, init captures), `mutable`, `std::function` vs template parameters, IILE, constexpr lambdas, template lambdas |
| 08 | [Const Correctness & Type System](./08-const-correctness-type-system/) | `const`, `constexpr`, `consteval`, `constinit`, `mutable`, static/dynamic/const/reinterpret casts, type traits |
| 09 | [Undefined Behavior & Pitfalls](./09-undefined-behavior-pitfalls/) | UB categories, signed overflow, strict aliasing, dangling references, sequence points, order of evaluation traps |
| 10 | [Exception Handling & noexcept](./10-exception-handling-noexcept/) | Exception safety guarantees (basic, strong, noexcept), stack unwinding mechanics, `noexcept` vector reallocations |
| 11 | [Design Patterns](./11-design-patterns/) | Modern Creational/Structural/Behavioral patterns, CRTP, Pimpl idiom, Type Erasure, Policy-based design |
| 12 | [Modern C++ Features](./12-modern-cpp-features/) | Complete standard features across C++17, C++20, and C++23 (`std::optional`, `std::variant`, modules, coroutines, `deducing this`) |
| 13 | [STL Algorithms & Ranges](./13-stl-algorithms-ranges/) | `<algorithm>` fluency, execution policies, C++20 Ranges pipeline (`views::filter`, `views::transform`), custom projections |
| 14 | [Low-Level Memory Layout & Optimization](./14-low-level-memory-layout-optimization/) | Struct alignment/padding (`alignas`), cache line false sharing, hardware prefetching, SIMD alignment, cache locality |
| 15 | [Tooling, Build Systems & Sanitizers](./15-tooling-build-systems-sanitizers/) | CMake target-based architecture, AddressSanitizer (ASan), ThreadSanitizer (TSan), UBSan, GDB/LLDB debugging, profiling |
| 16 | [Error Handling Strategies](./16-error-handling-strategies/) | `std::expected` (C++23), `std::error_code`, `system_error`, exception overhead vs error codes in HFT/real-time systems |
| 17 | [I/O, Formatting & Strings](./17-io-formatting-strings/) | Small String Optimization (SSO), `std::string_view` lifetime traps, `std::format` (C++20), `std::print` (C++23), fast I/O streams |
| 18 | [Standards Chronology (C++11–C++23)](./18-standards-chronology-cpp11-to-cpp23/) | Chronological standard-by-standard evolution, breaking changes, deprecations, and modern replacement idioms |
| 19 | [C++ Core Guidelines & Production Idioms](./19-cpp-core-guidelines-idioms/) | C++ Core Guidelines enforcement, resource safety rules, interface design, API stability, zero-overhead abstractions |
| 20 | [Build Systems, Toolchains & Package Managers](./20-build-systems-toolchains-package-managers/) | Modern CMake practices, compiler flags (`-O3`, `-flto`, `-march=native`), static analysis (`clang-tidy`), Conan & vcpkg package integration |

---

## Standard Module Structure

Every topic folder (`01` through `20`) is structured as follows:

```
NN-topic/
├── README.md              ← Module overview & technical syllabus
├── resources.md           ← Curated papers, documentation, and reading lists
├── theory-and-mechanics.md← Exhaustive deep-dive theory, assembly analysis, and internal mechanics
└── question-bank/
    ├── real-interview-q-a.md  ← Cold-drill problem set
    ├── easy.md                ← Foundational theory Q&A + solutions
    ├── medium.md              ← Intermediate engineering Q&A + solutions
    └── hard.md                ← Advanced / low-level Q&A + production implementations
```

