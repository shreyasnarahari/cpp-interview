# Modern C++ — Hard Questions (C++20/23)

---

**Q1. What are C++20 Concepts?**

A: Named constraints on template parameters — cleaner than SFINAE:

```cpp
// Define a concept:
template <typename T>
concept Numeric = std::is_arithmetic_v<T>;

template <typename T>
concept Printable = requires(T t) {
    { std::cout << t } -> std::same_as<std::ostream&>;
};

template <typename T>
concept Sortable = requires(T t) {
    { t.begin() } -> std::random_access_iterator;
    { t.end() }   -> std::random_access_iterator;
    requires std::totally_ordered<typename T::value_type>;
};

// Use:
template <Numeric T>
T add(T a, T b) { return a + b; }

// Or shorthand:
void print(const Printable auto& val) { std::cout << val; }

// Concept error messages are MUCH clearer than SFINAE errors
```

---

**Q2. What are C++20 Ranges?**

A: A composable, lazy pipeline for data processing — replaces iterator pairs:

```cpp
#include <ranges>

std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

// Filter even, square, take first 3:
auto result = v
    | std::views::filter([](int x) { return x % 2 == 0; })
    | std::views::transform([](int x) { return x * x; })
    | std::views::take(3);

for (int x : result) std::cout << x << " ";  // 4 16 36

// Lazy! No intermediate containers created.
// Infinite ranges:
for (int x : std::views::iota(1) | std::views::take(5)) {
    std::cout << x << " ";  // 1 2 3 4 5
}
```

---

**Q3. What is the spaceship operator (`<=>`, C++20)?**

A: Three-way comparison — generates all six comparison operators from one:

```cpp
struct Point {
    int x, y;
    auto operator<=>(const Point&) const = default;
    // Generates: ==, !=, <, >, <=, >=
};

Point a{1, 2}, b{3, 4};
if (a < b) { /* ... */ }   // works!
if (a == b) { /* ... */ }  // works!

// Custom ordering:
struct CaseInsensitiveString {
    std::string s;
    std::strong_ordering operator<=>(const CaseInsensitiveString& other) const {
        return toLower(s) <=> toLower(other.s);
    }
    bool operator==(const CaseInsensitiveString& other) const {
        return toLower(s) == toLower(other.s);
    }
};
```

Return types: `std::strong_ordering`, `std::weak_ordering`, `std::partial_ordering`.

---

**Q4. What is `std::expected` (C++23)?**

A: A Rust-style `Result<T, E>` — holds either a value or an error:

```cpp
std::expected<int, std::string> parseInt(std::string_view s) {
    try {
        return std::stoi(std::string(s));
    } catch (...) {
        return std::unexpected("invalid: " + std::string(s));
    }
}

auto result = parseInt("42");
if (result) {
    std::cout << *result;  // 42
} else {
    std::cerr << result.error();
}

// Monadic chaining:
auto final = parseInt("10")
    .transform([](int x) { return x * 2; })          // 20
    .transform([](int x) { return std::to_string(x); })  // "20"
    .transform_error([](auto e) { return "ERR: " + e; });
```

---

**Q5. What is deducing `this` (C++23)?**

A: Explicit object parameter — eliminates const/non-const overload duplication:

```cpp
// Before C++23 — must write two overloads:
class Widget {
    std::string name_;
public:
    const std::string& getName() const { return name_; }
    std::string& getName() { return name_; }
};

// C++23 — one function handles both:
class Widget {
    std::string name_;
public:
    template <typename Self>
    auto&& getName(this Self&& self) {
        return std::forward<Self>(self).name_;
    }
};
// const Widget → returns const string&
// Widget → returns string&
// Widget&& → returns string&&
```

Also enables recursive lambdas without Y-combinator:
```cpp
auto fib = [](this auto self, int n) -> int {
    if (n <= 1) return n;
    return self(n-1) + self(n-2);
};
```

---

## Coding Questions

---

**C1. Ranges pipeline**

A:
```cpp
// Top 3 longest words, uppercased
std::vector<std::string> words = {"hello", "world", "hi", "programming", "cpp", "interview"};

auto result = words
    | std::views::filter([](auto& s) { return s.size() > 3; })
    | std::views::transform([](auto s) {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return s;
    })
    | std::views::take(3);

for (const auto& w : result) std::cout << w << " ";
// HELLO WORLD PROGRAMMING
```

---

**C2. Concept-constrained generic function**

A:
```cpp
template <typename T>
concept Container = requires(T t) {
    typename T::value_type;
    { t.begin() } -> std::input_or_output_iterator;
    { t.end() } -> std::input_or_output_iterator;
    { t.size() } -> std::convertible_to<std::size_t>;
};

template <Container C>
void printAll(const C& container) {
    for (const auto& elem : container) {
        std::cout << elem << " ";
    }
    std::cout << "\n";
}

printAll(std::vector{1,2,3});   // OK
printAll(std::list{4,5,6});     // OK
// printAll(42);                // Concept error: int is not a Container
```

---

**C3. `std::format` (C++20)**

A:
```cpp
// Type-safe formatting (replaces printf and stringstreams):
std::string s = std::format("Hello, {}! You are {} years old.", "Alice", 30);
// "Hello, Alice! You are 30 years old."

// With format specifiers:
std::string hex = std::format("{:#x}", 255);      // "0xff"
std::string pad = std::format("{:>10}", "hi");     // "        hi"
std::string flt = std::format("{:.2f}", 3.14159);  // "3.14"

// C++23 std::print (replaces cout):
std::print("x = {}, y = {}\n", 42, 3.14);
```
