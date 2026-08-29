# Templates & Metaprogramming — Deep Theory & Mechanics

An exhaustive, low-level guide to C++ template instantiation mechanics, two-phase lookup, dependent names, SFINAE, C++20 Concepts & Constraints, variadic templates, fold expressions, type traits, and compile-time metaprogramming.

---

## 1. Template Compilation Model & Two-Phase Lookup

Templates are not executable code; they are **code-generation blueprints**. The compiler evaluates templates in **Two Distinct Phases**:

```
+-------------------------------------------------------------------------------+
| PHASE 1: TEMPLATE DEFINITION PARSING (Syntax & Non-Dependent Name Checking)   |
|                                                                               |
|  - Checked at template declaration time (before any instantiations).          |
|  - Validates basic C++ grammar and binds NON-DEPENDENT names.                 |
|  - Non-dependent types and functions must be declared before this point.      |
+---------------------------------------|---------------------------------------+
                                        v (Instantiation with concrete types)
+-------------------------------------------------------------------------------+
| PHASE 2: TEMPLATE INSTANTIATION (Dependent Name Binding & Code Generation)    |
|                                                                               |
|  - Triggered when template is invoked with concrete types (e.g. `Vector<int>`).|
|  - DEPENDENT names are looked up via Argument-Dependent Lookup (ADL).          |
|  - Emits concrete assembly functions in the object file (`.o`).               |
+-------------------------------------------------------------------------------+
```

### 1.1 Dependent Names & The `typename` Disambiguator

A name that depends on a template parameter `T` is an **unresolved dependent name**. By default, the C++ standard grammar assumes that any dependent qualified name `T::Something` is a **variable or static data member**, not a type!

```cpp
template <typename T>
void process() {
    // ERROR without `typename`: Compiler interprets `T::iterator * ptr` as multiplication!
    // (Value T::iterator multiplied by variable ptr)
    T::iterator* ptr; 

    // CORRECT: Explicitly informs the compiler that `iterator` is a nested type
    typename T::iterator* ptr; 
}
```

### 1.2 The `template` Disambiguator for Dependent Member Templates

Similarly, when calling a template member function on a dependent type, the `<` character is parsed as a "less-than" operator unless prefixed by the `template` keyword:

```cpp
template <typename T>
void execute(T& obj) {
    // ERROR without `template`: Parsed as `(obj.convert < int) > (42)`
    obj.convert<int>(42);

    // CORRECT:
    obj.template convert<int>(42);
}
```

---

## 2. Template Specialization vs Overloading

### 2.1 Full vs Partial Specialization

- **Class Templates**: Support **both** Full Specialization and Partial Specialization.
- **Function Templates**: Support **ONLY Full Specialization**. Function templates **CANNOT be partially specialized**!

```cpp
// Primary Class Template
template <typename T, typename U>
struct Pair {};

// 1. Partial Specialization: When second type is a pointer
template <typename T, typename U>
struct Pair<T, U*> {};

// 2. Partial Specialization: When both types are identical
template <typename T>
struct Pair<T, T> {};

// 3. Full Specialization: Exact concrete types
template <>
struct Pair<int, double> {};
```

### 2.2 Why You Should Overload Function Templates, NOT Specialize Them!

Specializations of function templates **do not participate in overload resolution**. Overload resolution considers only the primary templates; only *after* the best primary template is selected does the compiler check if a specialization exists!

```cpp
// Primary Template 1
template <typename T>
void print(T* p) { std::cout << "Pointer version\n"; }

// Primary Template 2
template <typename T>
void print(T val) { std::cout << "Value version\n"; }

// Full Specialization of Template 2 for int* (Trap!)
template <>
void print<int*>(int* p) { std::cout << "Specialized int*\n"; }

int* x = nullptr;
print(x); // PRINTS: "Pointer version"! (Template 1 was selected as best match,
          // so the specialization on Template 2 was completely ignored!)
```

**Rule**: Always prefer **regular function overloading** over function template specialization!

---

## 3. SFINAE & Substitution Failure Is Not An Error

### 3.1 What is SFINAE?
When deducing template arguments, if substituting a deduced type into the function signature causes an invalid type or expression, the compiler does **NOT** generate a compile error. Instead, it silently discards that candidate from the overload set!

### 3.2 `std::enable_if_t` Mechanics

```cpp
// Custom implementation of std::enable_if
template <bool Condition, typename T = void>
struct enable_if {};

template <typename T>
struct enable_if<true, T> { using type = T; };

template <bool Condition, typename T = void>
using enable_if_t = typename enable_if<Condition, T>::type;
```

#### Constraining Functions with SFINAE:
```cpp
// Enabled ONLY for Integral types
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
void process(T val) {
    std::cout << "Integer: " << val << "\n";
}

// Enabled ONLY for Floating-Point types
template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
void process(T val) {
    std::cout << "Float: " << val << "\n";
}
```

### 3.3 Expression SFINAE & The Detector Idiom (`std::void_t`)

```cpp
// Detects whether a type T has a `.serialize()` member function
template <typename T, typename = void>
struct has_serialize : std::false_type {};

template <typename T>
struct has_serialize<T, std::void_t<decltype(std::declval<T>().serialize())>> : std::true_type {};

template <typename T>
inline constexpr bool has_serialize_v = has_serialize<T>::value;
```

---

## 4. C++20 Concepts & Constraints

C++20 Concepts replace ugly SFINAE templates with clean, expressive, compile-time constraints and readable compiler error messages.

### 4.1 Concept Definition & `requires` Expressions

```cpp
template <typename T>
concept Numeric = std::is_integral_v<T> || std::is_floating_point_v<T>;

template <typename T>
concept Printable = requires(T a) {
    { std::cout << a } -> std::same_as<std::ostream&>;
};

template <typename T>
concept Serializable = requires(T a) {
    { a.serialize() } -> std::convertible_to<std::string>;
    typename T::serialized_type;
};
```

### 4.2 Four Syntaxes for Applying Concepts

```cpp
// 1. Constrained Template Parameter
template <Numeric T>
T add(T a, T b) { return a + b; }

// 2. Trailing Requires Clause
template <typename T>
requires Numeric<T> && Printable<T>
void calculate(T val);

// 3. Terse Auto Syntax (C++20)
auto multiply(Numeric auto a, Numeric auto b) { return a * b; }

// 4. In-place Ad-hoc Constraint
template <typename T>
requires requires(T x) { x.size(); }
void display_size(const T& container);
```

### 4.3 Concept Subsumption & Overload Resolution
When two overloads are both valid, the compiler automatically picks the **more constrained** concept (subsumption rule) without requiring manual disambiguation!

---

## 5. Variadic Templates & Fold Expressions

### 5.1 Parameter Packs & Pack Expansion
A variadic template accepts an arbitrary number of template arguments using `typename... Args` and expands them with `Args...`.

```cpp
template <typename... Args>
size_t count_arguments(Args... args) {
    return sizeof...(Args); // Returns number of types in pack at compile-time
}
```

### 5.2 The 4 Types of Fold Expressions (C++17)

| Fold Syntax | Expression | Expanded Form |
|---|---|---|
| **Unary Right Fold** | `(args + ...)` | `(arg1 + (arg2 + (arg3 + ...)))` |
| **Unary Left Fold** | `(... + args)` | `(((... + arg1) + arg2) + arg3)` |
| **Binary Right Fold** | `(args + ... + init)` | `(arg1 + (arg2 + (... + init)))` |
| **Binary Left Fold** | `(init + ... + args)` | `(((init + arg1) + arg2) + ...)` |

#### Practical Fold Expression Examples:
```cpp
// 1. Sum all arguments (Unary Left Fold)
template <typename... Args>
auto sum(Args... args) {
    return (... + args);
}

// 2. Print all arguments separated by spaces (Binary Left Fold)
template <typename... Args>
void print_all(const Args&... args) {
    ((std::cout << args << ' '), ...);
    std::cout << '\n';
}

// 3. Logical ALL / ANY check
template <typename... Args>
bool all_true(Args... args) {
    return (... && args);
}
```

---

## 6. Compile-Time Metaprogramming: `constexpr` vs `consteval`

- **`constexpr`**: Evaluated at compile-time if given compile-time constants; otherwise evaluated at **runtime**.
- **`consteval` (C++20 Immediate Functions)**: **Guaranteed** to execute exclusively at compile-time. Emits a compiler error if invoked in a runtime context.
- **`constinit` (C++20)**: Guarantees static/thread-local variable initialization at compile-time (eliminating the Static Initialization Order Fiasco).

```cpp
// Compile-time FNV-1a String Hashing
consteval uint64_t hash_string(std::string_view str) {
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : str) {
        hash = (hash ^ static_cast<uint64_t>(c)) * 0x100000001b3ULL;
    }
    return hash;
}

// Used in switch cases (which require compile-time integer constants!)
void handle_event(std::string_view event_name) {
    switch (hash_string(event_name)) { // Error if hash_string weren't compile-time!
        case hash_string("ORDER_NEW"):     /* ... */ break;
        case hash_string("ORDER_CANCEL"):  /* ... */ break;
    }
}
```
