# OOP & Polymorphism — Easy Questions

---

**Q1. What is the difference between `struct` and `class` in C++?**

A: The **only** difference is the default access specifier:
- `struct` — default `public`
- `class` — default `private`

```cpp
struct S { int x; };   // x is public
class C { int x; };    // x is private
```

Convention: use `struct` for plain data, `class` for types with invariants/behavior.

---

**Q2. What is a virtual function?**

A: A member function that can be **overridden** in derived classes, with the correct version called at **runtime** based on the actual object type (dynamic dispatch).

```cpp
class Animal {
public:
    virtual void speak() { std::cout << "...\n"; }
};
class Dog : public Animal {
public:
    void speak() override { std::cout << "Woof\n"; }
};

Animal* a = new Dog();
a->speak();  // "Woof" — dynamic dispatch calls Dog::speak
```

Without `virtual`, `a->speak()` would call `Animal::speak` (static dispatch).

---

**Q3. Why must a polymorphic base class destructor be virtual?**

A: If you `delete` a derived object through a base pointer and the destructor is NOT virtual, only `~Base()` is called — `~Derived()` is skipped → **resource leak or UB**.

```cpp
class Base {
public:
    virtual ~Base() = default;  // MUST be virtual
};
class Derived : public Base {
    std::vector<int> data;  // this would leak without virtual ~Base
};

Base* p = new Derived();
delete p;  // calls ~Derived() then ~Base() — correct
```

Rule: if a class has any virtual function, its destructor should be virtual.

---

**Q4. What is a pure virtual function?**

A: A virtual function with `= 0` — no implementation in the base class. Makes the class **abstract** (cannot be instantiated).

```cpp
class Shape {
public:
    virtual double area() const = 0;  // pure virtual
    virtual ~Shape() = default;
};

// Shape s;  // ERROR — cannot instantiate abstract class
class Circle : public Shape {
    double r;
public:
    Circle(double r) : r(r) {}
    double area() const override { return 3.14159 * r * r; }
};
```

---

**Q5. What is the `override` keyword?**

A: A compile-time check (C++11) ensuring a function actually overrides a base class virtual function. Catches typos and signature mismatches.

```cpp
class Base {
public:
    virtual void process(int x) {}
};
class Derived : public Base {
public:
    void process(double x) override {}  // COMPILE ERROR — wrong signature!
    // Without override, this silently creates a new function instead of overriding.
};
```

**Always use `override`** — it's free documentation and a safety net.

---

## Coding Questions

---

**C1. Shape hierarchy**

Implement `Shape` (abstract), `Circle`, and `Rectangle` with `area()` and `perimeter()`.

A:
```cpp
class Shape {
public:
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
    double radius_;
public:
    explicit Circle(double r) : radius_(r) {}
    double area() const override { return M_PI * radius_ * radius_; }
    double perimeter() const override { return 2.0 * M_PI * radius_; }
};

class Rectangle : public Shape {
    double w_, h_;
public:
    Rectangle(double w, double h) : w_(w), h_(h) {}
    double area() const override { return w_ * h_; }
    double perimeter() const override { return 2.0 * (w_ + h_); }
};

// Usage with polymorphism:
std::vector<std::unique_ptr<Shape>> shapes;
shapes.push_back(std::make_unique<Circle>(5.0));
shapes.push_back(std::make_unique<Rectangle>(3.0, 4.0));
for (const auto& s : shapes) {
    std::cout << "Area: " << s->area() << "\n";
}
```

---

**C2. Construction and destruction order**

Predict the output:

```cpp
struct A {
    A() { std::cout << "A "; } ~A() { std::cout << "~A "; }
};
struct B : A {
    B() { std::cout << "B "; } ~B() { std::cout << "~B "; }
};
struct C : B {
    C() { std::cout << "C "; } ~C() { std::cout << "~C "; }
};
{ C c; }
```

A: `A B C ~C ~B ~A` — Construction: base to derived. Destruction: derived to base (reverse order). This is always guaranteed in C++.

---

**C3. Fix the slicing bug**

```cpp
void printAnimal(Animal a) {  // BUG: pass by value
    a.speak();
}
```

A: Pass by reference (or pointer) to preserve polymorphism:
```cpp
void printAnimal(const Animal& a) {
    a.speak();  // dynamic dispatch works correctly
}
```

---

**C4. Implement a non-copyable base class**

A:
```cpp
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
};

class Connection : private NonCopyable {
    // Cannot be copied, can be moved
};
```
