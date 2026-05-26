# OOP & Polymorphism — Real Interview Questions

> No answers — attempt cold first, then check easy.md / medium.md / hard.md.

---

## Theory Questions

1. What is the difference between `struct` and `class` in C++?
2. What is a virtual function? Why do we need it?
3. What is a pure virtual function? What is an abstract class?
4. Why must a base class destructor be virtual when used polymorphically?
5. What is the vtable? How does virtual dispatch work at runtime?
6. What is object slicing? Give an example and explain how to avoid it.
7. What is the diamond problem in multiple inheritance? How does virtual inheritance solve it?
8. What is the Rule of Three? The Rule of Five? The Rule of Zero?
9. What is CRTP? When would you use it instead of virtual functions?
10. What is the `override` keyword? Why should you always use it?
11. What is the `final` keyword? When is it useful?
12. What is the difference between public, protected, and private inheritance?
13. Can you call a virtual function from a constructor? What happens?
14. What is a `friend` function? A `friend` class? When are they appropriate?
15. What is the order of construction and destruction in an inheritance hierarchy?
16. What is the `explicit` keyword? When should you use it on constructors?
17. What is a delegating constructor (C++11)?
18. Explain member initializer list order — does it match the list or declaration order?

---

## "What does this print?" Code Questions

```cpp
// Q1:
class Base {
public:
    Base() { init(); }
    virtual void init() { std::cout << "Base"; }
};
class Derived : public Base {
public:
    void init() override { std::cout << "Derived"; }
};
Derived d;  // What prints?
```

```cpp
// Q2:
class Animal {
public:
    virtual void speak() { std::cout << "..."; }
};
class Dog : public Animal {
public:
    void speak() override { std::cout << "Woof"; }
};
void listen(Animal a) { a.speak(); }  // pass by value
Dog d;
listen(d);  // What prints?
```

```cpp
// Q3:
class Base {
public:
    ~Base() { std::cout << "~Base "; }
};
class Derived : public Base {
public:
    ~Derived() { std::cout << "~Derived "; }
};
Base* p = new Derived();
delete p;  // What prints?
```

```cpp
// Q4:
class A {
public:
    A() { std::cout << "A"; }
    virtual ~A() { std::cout << "~A"; }
};
class B : public A {
public:
    B() { std::cout << "B"; }
    ~B() { std::cout << "~B"; }
};
class C : public B {
public:
    C() { std::cout << "C"; }
    ~C() { std::cout << "~C"; }
};
A* p = new C();
// What prints during construction? During delete?
delete p;
```

```cpp
// Q5:
struct Base {
    int x = 10;
    virtual void show() { std::cout << x; }
};
struct Derived : Base {
    int y = 20;
    void show() override { std::cout << y; }
};
Base b = Derived();  // What does b.show() print?
b.show();
```

---

## Implementation Problems

1. **Shape hierarchy**: Design an abstract `Shape` class with pure virtual `area()` and `perimeter()`. Implement `Circle`, `Rectangle`, and `Triangle`. Store them in a `vector<unique_ptr<Shape>>`.

2. **Clone pattern**: Implement a virtual `clone()` method that returns a `unique_ptr<Base>` for a polymorphic hierarchy. Demonstrate with at least 2 derived classes.

3. **CRTP static polymorphism**: Implement a logging framework using CRTP where different loggers (ConsoleLogger, FileLogger) share a common interface without virtual functions.

4. **Observer pattern**: Implement the observer pattern using an abstract `Observer` class and a `Subject` that manages observers.
