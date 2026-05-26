# OOP & Polymorphism — Hard Questions

---

**Q1. Explain the memory layout of a class with virtual functions.**

A: A polymorphic object has a hidden `vptr` (typically at offset 0) pointing to the class's vtable:

```cpp
class Base {
    int x;
public:
    virtual void f();
    virtual void g();
};

// sizeof(Base) = sizeof(vptr) + sizeof(int) + padding
// On 64-bit: 8 (vptr) + 4 (int) + 4 (padding) = 16 bytes

// Memory layout:
// Offset 0: vptr → Base's vtable
// Offset 8: int x
// Offset 12: 4 bytes padding (alignment to 8)
```

With inheritance:
```cpp
class Derived : public Base {
    int y;
public:
    void f() override;
};
// Offset 0: vptr → Derived's vtable (overwritten during construction)
// Offset 8: int x (inherited)
// Offset 12: int y
// Offset 16: padding
```

---

**Q2. What happens when you call a virtual function from a constructor?**

A: During construction of `Base`, the vtable pointer is set to `Base`'s vtable. The `Derived` part doesn't exist yet. So virtual calls in the constructor resolve to the **Base** version — no polymorphism.

```cpp
class Base {
public:
    Base() { speak(); }  // calls Base::speak, not Derived::speak
    virtual void speak() { std::cout << "Base\n"; }
};
class Derived : public Base {
public:
    void speak() override { std::cout << "Derived\n"; }
};
Derived d;  // prints "Base"
```

This is by design — calling `Derived::speak()` during `Base` construction would access uninitialized `Derived` members.

---

**Q3. Explain type erasure in C++.**

A: Type erasure hides concrete types behind a uniform interface, typically using virtual functions internally but exposing a value-type API externally.

`std::function` is the canonical example:

```cpp
// Simplified type erasure:
class AnyCallable {
    struct Concept {
        virtual void call() = 0;
        virtual ~Concept() = default;
    };

    template <typename F>
    struct Model : Concept {
        F func;
        Model(F f) : func(std::move(f)) {}
        void call() override { func(); }
    };

    std::unique_ptr<Concept> impl_;

public:
    template <typename F>
    AnyCallable(F f) : impl_(std::make_unique<Model<F>>(std::move(f))) {}

    void operator()() { impl_->call(); }
};

// Usage — stores ANY callable:
AnyCallable fn1 = []{ std::cout << "lambda\n"; };
AnyCallable fn2 = someFunction;
fn1();  // "lambda"
```

The key insight: templates at the boundary (constructor), virtual functions internally, value semantics externally.

---

**Q4. What is the overhead of multiple inheritance?**

A: With multiple inheritance, an object may have **multiple vptrs** — one for each base class with virtual functions:

```cpp
class A { virtual void fa(); };
class B { virtual void fb(); };
class C : public A, public B { void fa() override; void fb() override; };

// C's layout:
// Offset 0: vptr_A → C's vtable for A interface
// Offset 8: vptr_B → C's vtable for B interface
// sizeof(C) = 16 (two vptrs)
```

When casting `C*` to `B*`, the compiler adjusts the pointer to point to the `B` sub-object (adds offset). This pointer adjustment is a unique cost of multiple inheritance.

---

## Coding Questions

---

**C1. Type-erased function wrapper**

Implement a simplified `Function<R(Args...)>` using type erasure.

A:
```cpp
template <typename Signature>
class Function;

template <typename R, typename... Args>
class Function<R(Args...)> {
    struct Concept {
        virtual R invoke(Args... args) = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
        virtual ~Concept() = default;
    };

    template <typename F>
    struct Model : Concept {
        F func;
        Model(F f) : func(std::move(f)) {}
        R invoke(Args... args) override { return func(args...); }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(func);
        }
    };

    std::unique_ptr<Concept> impl_;

public:
    Function() = default;

    template <typename F>
    Function(F f) : impl_(std::make_unique<Model<F>>(std::move(f))) {}

    Function(const Function& other) : impl_(other.impl_ ? other.impl_->clone() : nullptr) {}
    Function(Function&&) noexcept = default;
    Function& operator=(Function other) {
        impl_ = std::move(other.impl_);
        return *this;
    }

    R operator()(Args... args) {
        return impl_->invoke(args...);
    }

    explicit operator bool() const { return impl_ != nullptr; }
};

// Usage:
Function<int(int, int)> add = [](int a, int b) { return a + b; };
std::cout << add(3, 4);  // 7
```

---

**C2. Virtual destructor quiz — predict all outputs**

```cpp
struct A {
    A() { std::cout << "A() "; }
    virtual ~A() { std::cout << "~A() "; }
};
struct B : A {
    B() { std::cout << "B() "; }
    ~B() override { std::cout << "~B() "; }
};
struct C : B {
    C() { std::cout << "C() "; }
    ~C() override { std::cout << "~C() "; }
};

A* p = new C();
std::cout << "| ";
delete p;
```

A: `A() B() C() | ~C() ~B() ~A()` — construction base-to-derived, destruction derived-to-base. Virtual destructor ensures the full chain is called through base pointer.

---

**C3. Implement Strategy pattern with runtime polymorphism**

A:
```cpp
class SortStrategy {
public:
    virtual void sort(std::vector<int>& data) = 0;
    virtual ~SortStrategy() = default;
};

class QuickSort : public SortStrategy {
public:
    void sort(std::vector<int>& data) override {
        std::sort(data.begin(), data.end());
    }
};

class BubbleSort : public SortStrategy {
public:
    void sort(std::vector<int>& data) override {
        for (size_t i = 0; i < data.size(); i++)
            for (size_t j = 0; j < data.size() - i - 1; j++)
                if (data[j] > data[j + 1])
                    std::swap(data[j], data[j + 1]);
    }
};

class Sorter {
    std::unique_ptr<SortStrategy> strategy_;
public:
    void setStrategy(std::unique_ptr<SortStrategy> s) {
        strategy_ = std::move(s);
    }
    void sort(std::vector<int>& data) {
        strategy_->sort(data);
    }
};
```

---

**C4. Diamond problem — fix and demonstrate**

```cpp
class Device {
public:
    int id;
    Device(int id) : id(id) { std::cout << "Device(" << id << ")\n"; }
};

class Printer : virtual public Device {
public:
    Printer(int id) : Device(id) { std::cout << "Printer\n"; }
};

class Scanner : virtual public Device {
public:
    Scanner(int id) : Device(id) { std::cout << "Scanner\n"; }
};

class AllInOne : public Printer, public Scanner {
public:
    // The most-derived class must initialize the virtual base
    AllInOne(int id) : Device(id), Printer(id), Scanner(id) {
        std::cout << "AllInOne\n";
    }
};

AllInOne a(1);
std::cout << a.id;  // Unambiguous — only one Device subobject
// Output: Device(1) Printer Scanner AllInOne
//         1
```
