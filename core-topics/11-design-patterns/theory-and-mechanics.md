# Design Patterns & Idioms — Deep Theory & Mechanics

An exhaustive, modern C++ guide to classic GoF design patterns, modern idioms, compile-time patterns (CRTP, Policy-Based Design), Type Erasure architectures, the Pimpl compilation firewall, and `std::variant` pattern matching.

---

## 1. Creational Patterns

### 1.1 Thread-Safe Meyers' Singleton
Since C++11 (§ [stmt.dcl]), block-scope static variables with initializers are guaranteed by the standard to be initialized in a **thread-safe manner** using internal compiler lock guards:

```cpp
class Singleton {
public:
    static Singleton& instance() {
        static Singleton s_instance; // Thread-safe dynamic initialization in C++11!
        return s_instance;
    }

    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

private:
    Singleton() = default;
};
```

---

### 1.2 The Builder Pattern with Fluent API
Separates complex object construction from its representation:

```cpp
class HttpRequest {
public:
    class Builder {
        std::string method_{"GET"};
        std::string url_;
        std::map<std::string, std::string> headers_;
    public:
        Builder& method(std::string m) { method_ = std::move(m); return *this; }
        Builder& url(std::string u) { url_ = std::move(u); return *this; }
        Builder& header(std::string k, std::string v) { headers_[k] = v; return *this; }
        HttpRequest build() { return HttpRequest(method_, url_, headers_); }
    };
private:
    HttpRequest(std::string m, std::string u, std::map<std::string, std::string> h);
};

auto req = HttpRequest::Builder()
    .method("POST")
    .url("https://api.exchange.com/order")
    .header("Content-Type", "application/json")
    .build();
```

---

## 2. Structural Patterns & The Pimpl Idiom

### 2.1 The Pimpl Idiom (Pointer to Implementation / Compilation Firewall)

```
Public Header (Widget.h - Fast Compilation, Stable ABI):
+------------------------------------+
| class Widget {                     |
|     class Impl;                    |  <-- Forward Declaration ONLY!
|     std::unique_ptr<Impl> pImpl_;  |
| public:                            |
|     void do_work();                |
| };                                 |
+------------------------------------+
                   |
                   v (Points to Heap Allocation)
Implementation File (Widget.cpp - Private):
+-------------------------------------------------------+
| class Widget::Impl {                                  |
|     HeavyInternalLibrary3rdParty lib_;                |
|     std::vector<ComplexSecretStruct> state_;          |
| public:                                               |
|     void do_work() { /* ... */ }                      |
| };                                                    |
+-------------------------------------------------------+
```

#### Why Use Pimpl?
1. **Compilation Firewall**: Changing private implementation details in `Widget.cpp` does **NOT** recompile downstream translation units that include `Widget.h`!
2. **ABI Stability**: The size and layout of `Widget` (`sizeof == 8` bytes) remain perfectly constant across library versions.
3. **Information Hiding**: Excludes internal third-party headers (e.g. Windows/Boost headers) from public API consumers.

*Rule: `~Widget()` must be declared in the header and defined in the `.cpp` file where `sizeof(Impl)` is complete!*

---

## 3. Behavioral Patterns: Visitor vs `std::variant`

### 3.1 Classic GoF Visitor (Double Dispatch)
Requires modifying base interfaces and adds virtual dispatch overhead:

```cpp
class Visitor;
class Circle;
class Square;

class Shape { public: virtual void accept(Visitor& v) = 0; };
class Circle : public Shape { public: void accept(Visitor& v) override { v.visit(*this); } };
```

### 3.2 Modern C++17 Visitor via `std::variant` & `std::visit`
Non-intrusive, zero-inheritance, value-semantic pattern matching:

```cpp
struct Circle { double radius; };
struct Square { double side; };
using Shape = std::variant<Circle, Square>;

// Overloaded Visitor Helper
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

double calculate_area(const Shape& s) {
    return std::visit(overloaded {
        [](const Circle& c) { return 3.14159 * c.radius * c.radius; },
        [](const Square& sq) { return sq.side * sq.side; }
    }, s);
}
```

---

## 4. Modern C++ Meta-Patterns: Type Erasure

Type Erasure bridges compile-time generic types with runtime dynamic polymorphism **without requiring inheritance hierarchies**:

```
+-------------------------------------------------------------------------------+
| TYPE-ERASED CONTAINER (e.g. AnyFunction / std::function / std::any)           |
|                                                                               |
|  1. Public Value Interface (Copyable, Movable)                                |
|  2. Internal Concept Interface (`struct Concept { virtual void call() = 0; }`)|
|  3. Templated Model Implementation:                                           |
|     `template <typename ConcreteT> struct Model : Concept { ... }`            |
+-------------------------------------------------------------------------------+
```

```cpp
class AnyCallable {
    struct Concept {
        virtual ~Concept() = default;
        virtual void invoke() = 0;
    };

    template <typename T>
    struct Model : public Concept {
        T callable_;
        Model(T c) : callable_(std::move(c)) {}
        void invoke() override { callable_(); }
    };

    std::unique_ptr<Concept> ptr_;

public:
    template <typename CallableT>
    AnyCallable(CallableT c) : ptr_(std::make_unique<Model<CallableT>>(std::move(c))) {}

    void operator()() { ptr_->invoke(); }
};
```
