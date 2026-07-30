# Theory & Mechanics: Design Patterns in Modern C++

## 1. Meyer's Singleton — Thread-Safety Guarantee (C++11)

C++11 guarantees that **local static variable initialization is thread-safe** (§6.7/4). The compiler emits a hidden guard variable with double-checked locking:

```cpp
class Singleton {
public:
    static Singleton& instance() {
        static Singleton s;  // thread-safe since C++11 — guaranteed by the standard
        return s;
    }
private:
    Singleton() = default;
};

// Compiler generates approximately:
static Singleton& instance() {
    static bool __guard = false;
    static char __storage[sizeof(Singleton)];

    if (!__guard) {                          // fast path — no lock
        __lock(__guard_mutex);               // slow path — lock
        if (!__guard) {                      // double-check
            new (__storage) Singleton();
            __guard = true;
        }
        __unlock(__guard_mutex);
    }
    return *reinterpret_cast<Singleton*>(__storage);
}
```

**When NOT to use Singleton:** When the "single instance" requirement is artificial. Singletons make testing hard (global state), hide dependencies, and complicate destruction ordering. Prefer dependency injection.

## 2. Pimpl Idiom — Compilation Firewall

Pimpl hides private members behind a pointer to an opaque type, providing **ABI stability** and **faster compilation** (changes to private members don't recompile dependents):

```cpp
// Widget.h — public header (stable ABI):
#include <memory>
class Widget {
public:
    Widget();
    ~Widget();  // MUST be declared here, defined in .cpp
    Widget(Widget&&) noexcept;             // move ops must be declared
    Widget& operator=(Widget&&) noexcept;  // for unique_ptr<incomplete>
    void do_work();
private:
    struct Impl;                 // forward declaration — incomplete type
    std::unique_ptr<Impl> pImpl; // unique_ptr works with incomplete types
};                               // (but destructor must be in .cpp where Impl is complete)

// Widget.cpp — implementation (can change freely without recompiling clients):
struct Widget::Impl {
    std::vector<int> data;       // adding/removing members here doesn't affect ABI
    std::string name;
    DatabaseConnection db;       // heavyweight header only included in .cpp
};

Widget::Widget() : pImpl(std::make_unique<Impl>()) {}
Widget::~Widget() = default;    // defined HERE where Impl is complete
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

void Widget::do_work() {
    pImpl->db.query(pImpl->data);
}
```

**Cost:** One extra heap allocation (for Impl) + one pointer indirection per method call. Negligible for most applications, but relevant for hot paths.

## 3. Type Erasure — The Concept/Model Pattern

`std::function`, `std::any`, and custom type-erased wrappers all use the same internal pattern:

```
Architecture:
┌──────────────────────────────────┐
│ TypeErased<Interface>            │
│   unique_ptr<ConceptBase> ptr ───────→ ┌──────────────────────────┐
└──────────────────────────────────┘     │ Model<ConcreteType>      │
                                         │   ConcreteType object    │
                                         │   virtual invoke() →     │
                                         │     object.invoke()      │
                                         └──────────────────────────┘
```

```cpp
// Example: type-erased callable (simplified std::function)
class AnyCallable {
    // Abstract base — the "Concept":
    struct Concept {
        virtual ~Concept() = default;
        virtual int call(int) = 0;
    };

    // Concrete wrapper — the "Model" (templated):
    template <typename F>
    struct Model : Concept {
        F func;
        Model(F f) : func(std::move(f)) {}
        int call(int x) override { return func(x); }
    };

    std::unique_ptr<Concept> ptr_;

public:
    // Template constructor accepts ANY callable matching the signature:
    template <typename F>
    AnyCallable(F f) : ptr_(std::make_unique<Model<F>>(std::move(f))) {}

    int operator()(int x) { return ptr_->call(x); }
};

// Usage — stores lambda, function pointer, functor — all through one type:
AnyCallable f1 = [](int x) { return x * 2; };
AnyCallable f2 = &some_function;
```

## 4. Policy-Based Design — Compile-Time Strategy Selection

Instead of virtual dispatch (runtime overhead), use **template parameters as policies**:

```cpp
// Strategy via virtual dispatch (runtime, ~5ns overhead per call):
class SortStrategy { public: virtual void sort(Data&) = 0; };
class QuickSort : public SortStrategy { void sort(Data& d) override { /*...*/ } };

// Strategy via policy template (compile-time, zero overhead):
template <typename SortPolicy, typename LogPolicy>
class DataProcessor : private SortPolicy, private LogPolicy {
public:
    void process(Data& d) {
        SortPolicy::sort(d);    // statically resolved, inlined
        LogPolicy::log("done"); // statically resolved, inlined
    }
};

// Usage — policies composed at compile time:
using FastProcessor = DataProcessor<QuickSort, NullLogger>;    // no logging overhead
using DebugProcessor = DataProcessor<QuickSort, FileLogger>;   // with file logging
```

Policy-based design trades **runtime flexibility** (can't change policy after compilation) for **zero-overhead abstraction** (all calls are inlined).
