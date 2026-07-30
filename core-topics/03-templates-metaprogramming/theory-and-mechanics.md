# Theory & Mechanics: Templates & Metaprogramming

## 1. Template Instantiation & Two-Phase Lookup

Templates are not code — they are **blueprints**. The compiler processes them in two phases:

**Phase 1 (Definition time):** Non-dependent names are looked up immediately.
```cpp
template <typename T>
void foo(T x) {
    bar();     // non-dependent — looked up NOW (must be visible)
    x.baz();   // dependent on T — deferred to Phase 2
}
```

**Phase 2 (Instantiation time):** Dependent names are looked up when `T` is known. Both the definition context AND the instantiation context are searched (ADL applies).

**Implicit instantiation** happens on first use. Each unique set of template arguments generates a **separate copy** of the function/class in the object file. This is why templates can cause **code bloat**.

## 2. SFINAE — Substitution Failure Is Not An Error

When the compiler tries to substitute template arguments and the substitution creates an **invalid type**, that overload is silently removed from the candidate set instead of causing a hard error.

### `std::enable_if` mechanics:

```cpp
// enable_if is trivially simple:
template <bool Cond, typename T = void>
struct enable_if {};                          // primary: no type member

template <typename T>
struct enable_if<true, T> { using type = T; }; // specialization: has type

// Usage — function only exists for integral types:
template <typename T>
typename std::enable_if<std::is_integral_v<T>, T>::type
double_it(T val) { return val * 2; }

// When T = double:
//   enable_if<false, double>::type → no member 'type' → substitution failure
//   → this overload is removed (not an error)
```

### `std::void_t` detection idiom (C++17):

```cpp
// void_t maps ANY well-formed type arguments to void:
template <typename...> using void_t = void;

// Detect if T has a .size() method:
template <typename T, typename = void>
struct has_size : std::false_type {};

template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};

// If T::size() is invalid → void_t<...> fails → SFINAE → falls back to false_type
// If T::size() is valid   → void_t<...> = void → specialization matches → true_type
```

## 3. Overload Resolution Priority

When multiple overloads and templates are candidates:

```
1. Exact match non-template function       (highest priority)
2. Template specialization (exact match)
3. Template (deduced match)
4. Non-template with implicit conversion    (lowest priority)
```

```cpp
void f(int);              // (1) non-template
template<> void f<int>(int); // (2) specialization
template<typename T> void f(T); // (3) template

f(42);  // calls (1) — non-template exact match wins
f<int>(42); // calls (2) — explicit <int> forces template, specialization wins
```

## 4. C++20 Concepts — Replacing SFINAE

Concepts provide readable, composable constraints with **subsumption** ordering:

```cpp
template <typename T>
concept Integral = std::is_integral_v<T>;

template <typename T>
concept SignedIntegral = Integral<T> && std::is_signed_v<T>;

// Subsumption: SignedIntegral SUBSUMES Integral (it's more constrained)
// When both match, the more constrained overload wins:

template <Integral T>       void process(T) { /* general */ }
template <SignedIntegral T>  void process(T) { /* specific */ }

process(42);   // calls SignedIntegral version (more constrained wins)
process(42u);  // calls Integral version (unsigned doesn't match SignedIntegral)
```

Unlike SFINAE, concepts produce **clear error messages** when constraints are not satisfied.

## 5. Fold Expressions (C++17)

Fold expressions expand parameter packs with an operator in a single expression:

```cpp
// Unary right fold: (pack op ...)
// Expands to: a1 op (a2 op (a3 op (... op aN)))

template <typename... Args>
auto sum(Args... args) {
    return (args + ...);  // unary right fold
}
// sum(1, 2, 3, 4) expands to: 1 + (2 + (3 + 4))

// Binary left fold with init: (init op ... op pack)
template <typename... Args>
void print_all(Args&&... args) {
    (std::cout << ... << args) << '\n';  // binary left fold
}
// print_all(1, " hello", 3.14) expands to:
// ((std::cout << 1) << " hello") << 3.14 << '\n'
```

## 6. Compile-Time Computation

```cpp
// constexpr — CAN be evaluated at compile time:
constexpr int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
constexpr int f5 = factorial(5);  // computed at compile time: 120
int runtime_val = 6;
int f6 = factorial(runtime_val);  // computed at runtime (also legal)

// consteval (C++20) — MUST be evaluated at compile time:
consteval int ct_factorial(int n) {
    return n <= 1 ? 1 : n * ct_factorial(n - 1);
}
constexpr int f7 = ct_factorial(7);   // OK — compile time
// int f8 = ct_factorial(runtime_val); // ERROR — cannot call consteval at runtime

// constinit (C++20) — compile-time INITIALIZATION, runtime use:
constinit int global = factorial(10);  // initialized at compile time
// global++;  // OK — can be modified at runtime (unlike constexpr)
```

`constinit` solves the **static initialization order fiasco** — it guarantees the variable is initialized before any dynamic initialization runs.
