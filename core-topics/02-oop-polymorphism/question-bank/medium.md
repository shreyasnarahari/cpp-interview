# OOP & Polymorphism — Medium Questions

---

**Q1. What is the vtable mechanism?**

A: Each class with at least one virtual function gets a **vtable** (virtual function table) — a static array of function pointers. Each object of that class stores a hidden **vptr** pointing to its class's vtable.

```
Dog object:        Dog's vtable:
┌──────────┐       ┌────────────────┐
│ vptr ─────────→  │ &Dog::speak    │  slot 0
│ name     │       │ &Dog::~Dog     │  slot 1
│ ...      │       └────────────────┘
└──────────┘

Animal object:     Animal's vtable:
┌──────────┐       ┌────────────────┐
│ vptr ─────────→  │ &Animal::speak │  slot 0
│ ...      │       │ &Animal::~Animal│ slot 1
└──────────┘       └────────────────┘
```

Virtual call: `a->speak()` → load vptr → index into vtable → call function pointer. Cost: ~1 indirect function call.

---

**Q2. What is object slicing?**

A: When a derived object is **copied by value** into a base object, the derived part is "sliced off":

```cpp
class Base { public: virtual void f() { std::cout << "Base"; } };
class Derived : public Base {
    int extra = 42;
public:
    void f() override { std::cout << "Derived " << extra; }
};

Derived d;
Base b = d;  // SLICED — b is a Base, not a Derived
b.f();       // "Base" — no polymorphism
```

Fix: always use pointers or references for polymorphic types.

---

**Q3. What is the diamond problem?**

A: When two classes inherit from the same base, and a third class inherits from both:

```
    A
   / \
  B   C
   \ /
    D
```

Without virtual inheritance, D has **two copies** of A — ambiguous access.

```cpp
class A { public: int x; };
class B : public A {};
class C : public A {};
class D : public B, public C {};

D d;
// d.x = 10;    // ERROR — ambiguous: B::x or C::x?
d.B::x = 10;    // OK but ugly

// Fix: virtual inheritance
class B : virtual public A {};
class C : virtual public A {};
class D : public B, public C {};
D d;
d.x = 10;  // OK — only one A subobject
```

---

**Q4. What is CRTP (Curiously Recurring Template Pattern)?**

A: A pattern where a class inherits from a template parameterized by itself, enabling **static polymorphism** (no vtable overhead):

```cpp
template <typename Derived>
class Printable {
public:
    void print() const {
        static_cast<const Derived*>(this)->printImpl();
    }
};

class Widget : public Printable<Widget> {
public:
    void printImpl() const { std::cout << "Widget\n"; }
};

class Gadget : public Printable<Gadget> {
public:
    void printImpl() const { std::cout << "Gadget\n"; }
};
```

Used in: `std::enable_shared_from_this`, Eigen library, compile-time optimization.

---

**Q5. Explain member initializer list order.**

A: Members are initialized in **declaration order** in the class, NOT the order they appear in the initializer list.

```cpp
class Widget {
    int b;  // declared first
    int a;  // declared second
public:
    Widget(int val) : a(val), b(a) {}  // WARNING: b is initialized before a!
    // b gets uninitialized a → UB
};
```

Always write initializer list in the same order as member declarations. Most compilers warn about this with `-Wreorder`.

---

## Coding Questions

---

**C1. Virtual clone pattern**

Implement a polymorphic `clone()` that returns a `unique_ptr<Base>`.

A:
```cpp
class Shape {
public:
    virtual std::unique_ptr<Shape> clone() const = 0;
    virtual double area() const = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
    double r_;
public:
    explicit Circle(double r) : r_(r) {}
    std::unique_ptr<Shape> clone() const override {
        return std::make_unique<Circle>(*this);
    }
    double area() const override { return M_PI * r_ * r_; }
};

// Usage:
std::unique_ptr<Shape> original = std::make_unique<Circle>(5.0);
auto copy = original->clone();  // polymorphic deep copy
```

---

**C2. Observer pattern**

A:
```cpp
class Observer {
public:
    virtual void onEvent(const std::string& event) = 0;
    virtual ~Observer() = default;
};

class Subject {
    std::vector<Observer*> observers_;
public:
    void subscribe(Observer* obs) { observers_.push_back(obs); }

    void unsubscribe(Observer* obs) {
        observers_.erase(
            std::remove(observers_.begin(), observers_.end(), obs),
            observers_.end());
    }

    void notify(const std::string& event) {
        for (auto* obs : observers_) {
            obs->onEvent(event);
        }
    }
};

class Logger : public Observer {
public:
    void onEvent(const std::string& event) override {
        std::cout << "[LOG] " << event << "\n";
    }
};
```

---

**C3. Rule of Five implementation**

Implement a `Matrix` class with proper copy, move, destructor, and assignment operators.

A:
```cpp
class Matrix {
    double* data_;
    size_t rows_, cols_;
public:
    Matrix(size_t r, size_t c)
        : data_(new double[r * c]()), rows_(r), cols_(c) {}

    ~Matrix() { delete[] data_; }

    Matrix(const Matrix& other)
        : data_(new double[other.rows_ * other.cols_]),
          rows_(other.rows_), cols_(other.cols_) {
        std::copy(other.data_, other.data_ + rows_ * cols_, data_);
    }

    Matrix(Matrix&& other) noexcept
        : data_(other.data_), rows_(other.rows_), cols_(other.cols_) {
        other.data_ = nullptr;
        other.rows_ = other.cols_ = 0;
    }

    Matrix& operator=(Matrix other) {  // copy-and-swap
        swap(*this, other);
        return *this;
    }

    friend void swap(Matrix& a, Matrix& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.rows_, b.rows_);
        swap(a.cols_, b.cols_);
    }

    double& operator()(size_t r, size_t c) { return data_[r * cols_ + c]; }
    const double& operator()(size_t r, size_t c) const { return data_[r * cols_ + c]; }
};
```

---

**C4. CRTP Logger**

Implement a CRTP-based logging framework with ConsoleLogger and FileLogger.

A:
```cpp
template <typename Derived>
class Logger {
public:
    void log(const std::string& msg) {
        static_cast<Derived*>(this)->writeImpl("[LOG] " + msg);
    }
    void error(const std::string& msg) {
        static_cast<Derived*>(this)->writeImpl("[ERR] " + msg);
    }
};

class ConsoleLogger : public Logger<ConsoleLogger> {
public:
    void writeImpl(const std::string& msg) {
        std::cout << msg << "\n";
    }
};

class FileLogger : public Logger<FileLogger> {
    std::ofstream file_;
public:
    explicit FileLogger(const std::string& path) : file_(path) {}
    void writeImpl(const std::string& msg) {
        file_ << msg << "\n";
    }
};

// No virtual functions, no vtable — all dispatch is compile-time
template <typename L>
void doWork(Logger<L>& logger) {
    logger.log("Starting work");
    logger.error("Something failed");
}
```
