# Templates & Metaprogramming — Hard Questions

---

**Q1. What are C++20 Concepts?**

A: Named constraints on template parameters. Replace SFINAE with readable, first-class syntax:

```cpp
// SFINAE (old):
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
T double_it(T val) { return val * 2; }

// Concepts (new):
template <std::integral T>
T double_it(T val) { return val * 2; }

// Custom concept:
template <typename T>
concept Hashable = requires(T t) {
    { std::hash<T>{}(t) } -> std::convertible_to<std::size_t>;
};

template <Hashable T>
void insert(const T& key) { /* ... */ }
```

Concepts give better error messages, compose naturally, and are self-documenting.

---

**Q2. What is `std::void_t` and the detection idiom?**

A: `std::void_t<Ts...>` maps any type list to `void`. Used to detect if an expression is valid:

```cpp
// Primary: T does not have .size()
template <typename T, typename = void>
struct has_size : std::false_type {};

// Specialization: T has .size()
template <typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>>
    : std::true_type {};

static_assert(has_size<std::vector<int>>::value);
static_assert(!has_size<int>::value);
```

How it works: if `T.size()` is valid, the specialization is a better match (void = void). If invalid, SFINAE removes the specialization.

---

**Q3. Explain template argument deduction rules.**

A: Key rules:
1. Deduction from function arguments — compiler matches `T` against actual argument type
2. Reference collapsing: `T&& + lvalue` → `T&`, `T&& + rvalue` → `T&&`
3. Array/function decay applies (unless parameter is a reference)
4. cv-qualifiers are deduced as part of `T` when parameter is by value
5. `auto` follows the same rules (except braced-init-list → `std::initializer_list`)

```cpp
template <typename T> void f(T x);
f(42);           // T = int
f("hello");      // T = const char* (array decays)

template <typename T> void g(T& x);
g(42);           // ERROR — can't bind rvalue to lvalue ref
int arr[5];
g(arr);          // T = int[5] (no decay through reference)

template <typename T> void h(T&& x);  // forwarding reference
int i = 0;
h(i);            // T = int&  (lvalue → T& && = T&)
h(42);           // T = int   (rvalue → int&&)
```

---

**Q4. What is expression SFINAE?**

A: SFINAE based on the validity of arbitrary expressions, not just types:

```cpp
// Check if T supports <<
template <typename T>
auto print(const T& val) -> decltype(std::cout << val, void()) {
    std::cout << val;
}

// Fallback
void print(...) {
    std::cout << "[unprintable]";
}
```

The trailing return type `decltype(expr, void())` triggers SFINAE if `expr` is invalid.

---

## Coding Questions

---

**C1. Type list operations**

Implement a compile-time type list with `size` and `type_at<N>`:

A:
```cpp
template <typename... Ts>
struct TypeList {};

// Size
template <typename List>
struct Size;

template <typename... Ts>
struct Size<TypeList<Ts...>> {
    static constexpr size_t value = sizeof...(Ts);
};

// Type at index N
template <size_t N, typename List>
struct TypeAt;

template <typename Head, typename... Tail>
struct TypeAt<0, TypeList<Head, Tail...>> {
    using type = Head;
};

template <size_t N, typename Head, typename... Tail>
struct TypeAt<N, TypeList<Head, Tail...>> {
    using type = typename TypeAt<N - 1, TypeList<Tail...>>::type;
};

// Usage:
using MyList = TypeList<int, double, char>;
static_assert(Size<MyList>::value == 3);
static_assert(std::is_same_v<TypeAt<1, MyList>::type, double>);
```

---

**C2. `constexpr` string hash**

A:
```cpp
constexpr uint32_t fnv1a(const char* s, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= static_cast<uint32_t>(s[i]);
        hash *= 16777619u;
    }
    return hash;
}

constexpr uint32_t operator""_hash(const char* s, size_t len) {
    return fnv1a(s, len);
}

// Compile-time string hashing:
switch (commandHash) {
    case "GET"_hash:  handleGet();  break;
    case "POST"_hash: handlePost(); break;
}
```

---

**C3. Concept-constrained container adapter**

Write a concept `Sortable` and a function that sorts any container satisfying it:

A:
```cpp
template <typename C>
concept Sortable = requires(C& c) {
    { c.begin() } -> std::random_access_iterator;
    { c.end() }   -> std::random_access_iterator;
    { c.size() }  -> std::convertible_to<std::size_t>;
    requires std::totally_ordered<typename C::value_type>;
};

template <Sortable Container>
void sort_container(Container& c) {
    std::sort(c.begin(), c.end());
}

std::vector<int> v = {3, 1, 4, 1, 5};
sort_container(v);  // OK

std::list<int> l = {3, 1, 4};
// sort_container(l);  // Concept error: list doesn't have random access iterators
```

---

**C4. `make_unique` with perfect forwarding**

Implement `make_unique` from scratch:

A:
```cpp
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// Usage:
auto widget = make_unique<Widget>(42, "hello");
```

Key: `Args&&` are forwarding references, `std::forward` preserves value category of each argument. This is the canonical perfect forwarding pattern.
