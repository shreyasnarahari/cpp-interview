# Modern C++ — Medium Questions (C++17)

---

**Q1. What are structured bindings?**

A: Decompose structs, pairs, tuples, and arrays into named variables:

```cpp
// Pairs/tuples:
auto [name, age] = std::make_pair("Alice", 30);

// Maps:
std::map<std::string, int> scores = {{"Alice", 95}, {"Bob", 87}};
for (const auto& [name, score] : scores) {
    std::cout << name << ": " << score << "\n";
}

// Structs:
struct Point { double x, y, z; };
auto [x, y, z] = Point{1.0, 2.0, 3.0};

// Arrays:
int arr[] = {1, 2, 3};
auto [a, b, c] = arr;

// With if-init:
if (auto [iter, inserted] = myMap.insert({key, value}); !inserted) {
    std::cout << "Key already exists\n";
}
```

---

**Q2. What is `std::optional`?**

A: A wrapper that may or may not contain a value — replaces "magic values" and output parameters:

```cpp
std::optional<int> findIndex(const std::vector<int>& v, int target) {
    for (size_t i = 0; i < v.size(); i++) {
        if (v[i] == target) return i;  // contains value
    }
    return std::nullopt;  // empty
}

auto idx = findIndex(v, 42);
if (idx) {                           // or idx.has_value()
    std::cout << "Found at " << *idx;  // or idx.value()
} else {
    std::cout << "Not found";
}

// value_or for default:
int pos = findIndex(v, 42).value_or(-1);
```

---

**Q3. What is `std::variant`?**

A: A type-safe union — holds exactly one of the specified types:

```cpp
std::variant<int, double, std::string> v;
v = 42;              // holds int
v = 3.14;            // now holds double
v = "hello"s;        // now holds string

// Access with std::get:
std::string s = std::get<std::string>(v);  // OK
// std::get<int>(v);  // throws std::bad_variant_access

// Safe access with std::visit:
std::visit([](const auto& val) { std::cout << val; }, v);

// Check which type:
if (std::holds_alternative<int>(v)) { /* ... */ }
std::cout << v.index();  // 0=int, 1=double, 2=string
```

---

**Q4. What is `std::string_view`?**

A: A non-owning reference to a string — no allocation, no copy:

```cpp
void process(std::string_view sv) {
    // sv doesn't own the data — just a pointer + size
    std::cout << sv.substr(0, 5);
}

process("hello world");         // no std::string construction
process(std::string("hello"));  // no copy

// WARNING: don't return string_view to a temporary:
// std::string_view bad() { return std::string("temp"); }  // dangling!
```

---

**Q5. What is `[[nodiscard]]`?**

A: Compiler warning if a return value is discarded:

```cpp
[[nodiscard]] int computeResult() { return 42; }
computeResult();  // WARNING: discarding return value

// Great for error codes:
[[nodiscard("Check the error code")]]
ErrorCode initialize();

// On classes (C++17):
struct [[nodiscard]] ErrorCode { int code; };
ErrorCode doWork();
doWork();  // WARNING — must check
```

---

## Coding Questions

---

**C1. Error handling with `std::optional`**

A:
```cpp
struct Config {
    std::string host;
    int port;
};

std::optional<Config> loadConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file) return std::nullopt;
    Config c;
    if (!(file >> c.host >> c.port)) return std::nullopt;
    return c;
}

auto config = loadConfig("config.txt");
std::string host = config.transform([](auto& c) { return c.host; })
                        .value_or("localhost");
```

---

**C2. `std::variant` state machine**

A:
```cpp
struct Idle {};
struct Running { int progress; };
struct Done { std::string result; };
struct Error { std::string message; };

using State = std::variant<Idle, Running, Done, Error>;

State advance(const State& current) {
    return std::visit(overloaded{
        [](const Idle&) -> State { return Running{0}; },
        [](const Running& r) -> State {
            if (r.progress >= 100) return Done{"Complete"};
            return Running{r.progress + 10};
        },
        [](const Done& d) -> State { return d; },
        [](const Error& e) -> State { return e; },
    }, current);
}
```

---

**C3. `if` with initializer**

A:
```cpp
// C++17: if (init; condition)
if (auto it = map.find(key); it != map.end()) {
    process(it->second);
    // 'it' scoped to this if block
}

// Works with switch too:
switch (auto val = compute(); val) {
    case 0: std::cout << "zero"; break;
    case 1: std::cout << "one"; break;
    default: std::cout << val;
}
```
