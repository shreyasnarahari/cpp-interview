# Theory & Mechanics: Modern C++ Design Patterns

## 1. Creational Patterns

### 1. Meyer's Singleton (Thread-Safe Magic Static)
In C++11 and later, initialization of function-local static variables is **guaranteed to be thread-safe by the ISO language standard** ("Magic Statics").

```cpp
class Singleton {
private:
    Singleton() = default;
    ~Singleton() = default;
public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    static Singleton& instance() {
        static Singleton inst; // Thread-safe 1st-call initialization!
        return inst;
    }
};
```
*Under the hood*: The compiler wraps local static initialization with an internal atomic flag check (`__cxa_guard_acquire` / `__cxa_guard_release`).

### 2. Factory Method vs Abstract Factory
- **Factory Method**: Defines a virtual creation interface in a base class, deferring instantiation to derived classes (`virtual unique_ptr<Product> createProduct()`).
- **Abstract Factory**: Provides an interface for creating **families of related or dependent objects** without specifying their concrete classes (`createButton()`, `createScrollbar()`).

---

## 2. Structural Patterns

### 1. The Pimpl Idiom (Pointer to Implementation)
Hide internal private member variables and includes from header files to achieve:
1. **Compilation Firewall**: Modifying private implementation details in `Widget.cpp` does not trigger re-compilation of files including `Widget.h`.
2. **ABI Stability**: Class memory size remains constant (`sizeof(Widget) == sizeof(unique_ptr)`), preserving binary compatibility across library updates.

```cpp
// Widget.h
#include <memory>
class Widget {
public:
    Widget();
    ~Widget(); // MUST be declared in header, defined in .cpp!
    void do_work();
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};
```

### 2. Decorator Pattern
Dynamically attaches additional responsibilities to an object at runtime or compile-time without modifying the underlying class interface.

---

## 3. Behavioral Patterns

### 1. Strategy Pattern: Dynamic vs Static (CRTP)
Allows switching algorithms at runtime (virtual dispatch) or compile time (template parameters).

#### Dynamic Strategy (Runtime Virtual Dispatch):
```cpp
class LoggerStrategy { public: virtual ~LoggerStrategy() = default; virtual void log(std::string_view msg) = 0; };
class ConsoleLogger : public LoggerStrategy { public: void log(std::string_view msg) override { std::cout << msg; } };
```

#### Static Strategy (Policy-Based Design / CRTP):
```cpp
template <typename OutputPolicy>
class Logger : private OutputPolicy {
public:
    void log(std::string_view msg) {
        OutputPolicy::write(msg); // 0-cost inlined call!
    }
};
```

### 2. Observer Pattern & Safe Weak Pointer Lifetime Management
Subject notifies multiple observers when a state change occurs.

**Dangling Reference Trap**: If an observer is destroyed while registered with a subject, calling `observer->notify()` causes Use-After-Free UB!
**Fix**: Store observers as `std::weak_ptr<Observer>`:

```cpp
class Subject {
    std::vector<std::weak_ptr<Observer>> observers_;
public:
    void notify(const std::string& event) {
        for (auto it = observers_.begin(); it != observers_.end(); ) {
            if (auto sp = it->lock()) { // Safely promote weak_ptr!
                sp->on_notify(event);
                ++it;
            } else {
                it = observers_.erase(it); // Clean up dead observers automatically!
            }
        }
    }
};
```

### 3. Visitor Pattern & Modern `std::visit`
Double dispatch pattern allowing new virtual operations to be added to an existing class hierarchy without modifying classes. Modern C++ replaces hierarchy visitors with `std::variant` and `std::visit`:

```cpp
using Shape = std::variant<Circle, Rectangle>;

struct RenderVisitor {
    void operator()(const Circle& c) const { std::cout << "Circle"; }
    void operator()(const Rectangle& r) const { std::cout << "Rectangle"; }
};

Shape s = Circle{};
std::visit(RenderVisitor{}, s); // Double dispatch via type-matching!
```

---

## 4. Advanced C++ Idioms: CRTP & Type Erasure

### 1. CRTP (Curiously Recurring Template Pattern)
Derived class inherits from a template base instantiated with the derived class itself (`class Derived : public Base<Derived>`).
- **Use Cases**: Static polymorphism, mixin feature composition, avoiding vtable dispatch overhead.

### 2. Type Erasure (`std::function`, Custom `Any`)
Combines template constructors with internal non-template virtual interface wrappers to store ANY type matching an interface without requiring common base inheritance:

```cpp
class AnyPrintable {
    struct Concept {
        virtual ~Concept() = default;
        virtual void print() const = 0;
    };
    template <typename T>
    struct Model : Concept {
        T object_;
        Model(T obj) : object_(std::move(obj)) {}
        void print() const override { std::cout << object_ << "\n"; }
    };
    std::unique_ptr<Concept> pimpl_;
public:
    template <typename T>
    AnyPrintable(T obj) : pimpl_(std::make_unique<Model<T>>(std::move(obj))) {}
    void print() const { pimpl_->print(); }
};
```
