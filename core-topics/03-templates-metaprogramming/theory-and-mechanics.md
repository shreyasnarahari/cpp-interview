# Theory & Mechanics: Templates & Metaprogramming

## 1. Template Instantiation & Overload Resolution

### Instantiation Model
Templates are not compiled into machine code until **instantiated** with concrete types:
- **Implicit Instantiation**: The compiler generates code when a template is used (`std::vector<int> v;`).
- **Explicit Instantiation**: Forces compilation of a specific instantiation (`template class std::vector<double>;`).
- **Template Code Bloat**: Instantiating `MyTemplate<T>` for 50 distinct types generates 50 separate copies of binary instructions.

### Overload Resolution Rules
When a function call matches both template and non-template overloads, the compiler follows a strict precedence:

1. **Non-Template Function Exact Match**: Takes precedence over any template!
2. **Template Function Specialization**: If a template is explicitly specified (`f<int>(42)`).
3. **Primary Template Deduction**: Deduces types dynamically.

```cpp
template <typename T> void f(T)    { std::cout << "template\n"; }
template <>           void f(int)  { std::cout << "specialized\n"; }
void                       f(int)  { std::cout << "non-template\n"; }

f(42);      // Calls non-template -> prints "non-template"
f<int>(42); // Explicit specialization -> prints "specialized"
f(3.14);    // Deduces double -> prints "template"
```

**CRITICAL RULE**: Function templates **CANNOT be partially specialized**! Only class templates and variable templates support partial specialization. For function templates, use overloading or C++20 Concepts.

---

## 2. SFINAE (Substitution Failure Is Not An Error)

### Core Mechanics
When substituting deduced template arguments into a function signature fails, the compiler does **not** emit a compilation error. Instead, it quietly discards that candidate overload from the candidate set.

### `std::enable_if` Implementation
```cpp
template <bool B, typename T = void>
struct enable_if {}; // Primary template (B == false): no ::type defined!

template <typename T>
struct enable_if<true, T> { using type = T; }; // Specialization (B == true)

template <bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;
```

#### SFINAE Usage Example:
```cpp
// Only enabled for integral types
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
void process(T val) {
    std::cout << "Integer: " << val << "\n";
}
```

### Detection Idiom via `std::void_t` (C++17)
`std::void_t<Ts...>` maps any list of valid types to `void`. If any expression inside `void_t` is invalid, substitution fails (SFINAE):

```cpp
template <typename T, typename = void>
struct has_serialize : std::false_type {};

template <typename T>
struct has_serialize<T, std::void_t<decltype(std::declval<T>().serialize())>> 
    : std::true_type {};
```

---

## 3. C++20 Concepts & Constraints

Concepts replace verbose, cryptic SFINAE templates with readable, compile-time constraints and clear compiler diagnostics.

```cpp
#include <concepts>

// Concept definition
template <typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::same_as<size_t>;
};

// Constrained template function
template <Hashable T>
void print_hash(T obj) {
    std::cout << std::hash<T>{}(obj) << "\n";
}
```

### Four Forms of `requires` Clauses:
1. **Simple Requirement**: `a + b;` (verifies expression compiles).
2. **Type Requirement**: `typename T::value_type;` (verifies nested type exists).
3. **Compound Requirement**: `{ expr } -> Concept;` (verifies return type satisfies concept).
4. **Nested Requirement**: `requires sizeof(T) == 8;` (verifies boolean compile-time predicate).

---

## 4. Variadic Templates & Fold Expressions (C++17)

### Parameter Packs & `sizeof...`
Parameter packs (`Args... args`) accept zero or more template arguments. `sizeof...(args)` queries the pack size at compile time.

### Fold Expressions Syntax (C++17)
Replaces complex recursive variadic template instantiations with single-line expansion:

| Fold Type | Syntax | Expansion (`sum(1, 2, 3)`) |
|---|---|---|
| **Unary Right Fold** | `(pack op ...)` | `(1 + (2 + 3))` |
| **Unary Left Fold** | `(... op pack)` | `((1 + 2) + 3)` |
| **Binary Right Fold** | `(pack op ... op init)` | `(1 + (2 + (3 + 0)))` |
| **Binary Left Fold** | `(init op ... op pack)` | `(((0 + 1) + 2) + 3)` |

```cpp
template <typename... Args>
auto sum(Args... args) {
    return (... + args); // Unary Left Fold
}

template <typename... Args>
void print_all(Args&&... args) {
    (std::cout << ... << args) << "\n"; // Fold over binary operator<<
}
```

---

## 5. Compile-Time Evaluation: `constexpr`, `consteval`, `constinit`

| Keyword | Evaluation Timing | Primary Purpose |
|---|---|---|
| `constexpr` | Compile-time **if inputs are constant**; runtime otherwise | Functions/variables usable in compile-time contexts |
| `consteval` (C++20) | **GUARANTEED Compile-Time ONLY** (Immediate functions) | Enforces 0 runtime overhead execution |
| `constinit` (C++20) | Compile-time **initialization**, runtime value modification | Eliminates Static Initialization Order Fiasco |

```cpp
consteval int square(int x) { return x * x; }
constexpr int val = square(5); // OK - compiled at compile time
int n = 5;
// int runtime_val = square(n); // ERROR! consteval cannot take non-const runtime argument
```

---

## 6. Type Traits Implementation from Scratch

### 1. `std::is_same<T, U>`
```cpp
template <typename T, typename U>
struct is_same : std::false_type {};

template <typename T>
struct is_same<T, T> : std::true_type {};

template <typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;
```

### 2. `std::remove_reference<T>`
```cpp
template <typename T> struct remove_reference      { using type = T; };
template <typename T> struct remove_reference<T&>  { using type = T; };
template <typename T> struct remove_reference<T&&> { using type = T; };
```

---

## 7. Dependent Type Names & Disambiguators (`typename` & `template`)

When referencing a type or template inside a class template that depends on a template parameter `T`:

1. **`typename` Disambiguator**: Mandatory before dependent nested types (`typename T::value_type ptr;`). Without `typename`, the compiler assumes `value_type` is a static member variable (multiplication error).
2. **`template` Disambiguator**: Mandatory when calling a dependent member template function (`obj.template get<int>();`).

```cpp
template <typename T>
void process_container(T& container) {
    typename T::iterator it = container.begin(); // Requires 'typename'
    container.template convert<double>();         // Requires 'template'
}
```
