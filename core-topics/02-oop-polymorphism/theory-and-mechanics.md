# Theory & Mechanics: OOP & Polymorphism

## 1. Virtual Functions & `vtable` Mechanics

### Virtual Table (`vtable`) & Pointer (`vptr`) Layout
When a class declares or inherits at least one `virtual` function, the compiler inserts a hidden pointer member called `vptr` (typically at offset 0 of the object).
- **`vtable`**: A static array of function pointers per class created by the compiler during compilation.
- **`vptr`**: A pointer stored inside every instance of the object pointing to the class's `vtable`.

```
Polymorphic Object & vtable Layout:

Derived Object Instance (in Memory):       Derived Class vtable (in Read-Only Data Segment):
┌───────────────────────────────┐          ┌──────────────────────────────────┐
│ vptr ─────────────────────────┼─────────>│ Offset to Top (0)                │
├───────────────────────────────┤          │ RTTI Pointer (typeinfo Derived) │
│ Base Data Member (e.g. int x) │          ├──────────────────────────────────┤
├───────────────────────────────┤          │ &Derived::speak()                │
│ Derived Data Member (int y)   │          │ &Base::print()                   │
└───────────────────────────────┘          └──────────────────────────────────┘
```

### Dynamic Dispatch Assembly Mechanics
Calling a virtual function `ptr->speak()` involves **one pointer dereference + offset lookup**:

```assembly
; x86-64 Assembly for ptr->speak() where speak() is vtable slot 0:
mov rdi, QWORD PTR [rbp-8]    ; rdi = ptr (this pointer)
mov rax, QWORD PTR [rdi]      ; rax = ptr->vptr (load vtable address)
mov rax, QWORD PTR [rax]      ; rax = vtable[0] (load function address)
call rax                      ; Indirect function call
```
**Performance Cost**:
1. **Memory Dereferences**: Two cache accesses (`vptr` -> `vtable slot`).
2. **Branch Prediction**: Indirect call (`call rax`) prevents the CPU pipeline from speculatively decoding instructions unless the branch predictor warm-starts target addresses.
3. **Inlining Barrier**: Compilers cannot inline virtual functions unless **devirtualization** applies (`final` keyword or total-program analysis).

---

## 2. Virtual Dispatch in Constructors & Destructors (CRITICAL TRAP)

### Execution Behavior
Calling a virtual function from a constructor or destructor executes the **local class's implementation**, NOT the derived override!

```cpp
class Base {
public:
    Base() { init(); } // Calling virtual function inside Base constructor!
    virtual void init() { std::cout << "Base"; }
};

class Derived : public Base {
public:
    Derived() {}
    void init() override { std::cout << "Derived"; }
};

Derived d; // PRINTS "Base", NOT "Derived"!
```

### Underlying Mechanism
During construction, an object is built layer-by-layer from Base to Derived:
1. When `Base::Base()` executes, the derived object's constructor has not yet run, and `Derived` members are uninitialized.
2. The compiler explicitly sets the object's `vptr` to point to `Base::vtable` at the entry of `Base::Base()`.
3. Only after `Base::Base()` completes and `Derived::Derived()` begins does the compiler overwrite `vptr` to point to `Derived::vtable`.
4. Similarly, during destruction, `vptr` is reset back to `Base::vtable` before executing `Base::~Base()`.

---

## 3. Multiple & Virtual Inheritance

### Multiple Inheritance Layout & Adjustor Thunks
When a class inherits from multiple base classes (`class Derived : public BaseA, public BaseB`), the object contains **multiple `vptr`s**:

```
Object Layout (class Derived : public BaseA, public BaseB):
┌─────────────────────────────┐
│ vptr_BaseA                  │ ───> Derived vtable (BaseA view)
│ BaseA data                  │
├─────────────────────────────┤
│ vptr_BaseB                  │ ───> Derived vtable (BaseB view)
│ BaseB data                  │
├─────────────────────────────┤
│ Derived data                │
└─────────────────────────────┘
```

#### Adjustor Thunks:
When passing a `Derived*` to a function expecting `BaseB*`, the compiler adjusts the pointer by adding a byte offset: `base_b_ptr = derived_ptr + sizeof(BaseA)`.
If a virtual method overridden in `Derived` is called through `BaseB*`, the vtable uses an **adjustor thunk** instruction sequence to subtract the offset back from `this` before invoking `Derived::method()`.

### Virtual Inheritance & The Diamond Problem
Without virtual inheritance, inheriting from two classes that share a common base creates duplicate base instances and ambiguity:

```
    A                     A (vbase)
   / \                   / \
  B   C       ===>      B   C
   \ /                   \ /
    D                     D
(Duplicate A)       (Single Shared A)
```

#### Virtual Base Table (`vbtable` / `vbase_offset`):
With `class B : virtual public A`, class `B` does not embed `A` at a fixed compile-time offset. Instead, `A` is placed at the end of the complete derived object, and `B` stores a `vbase_offset` inside its `vtable` to locate `A` dynamically at runtime.

---

## 4. Object Slicing & Value Semantics

### What is Object Slicing?
Object slicing occurs when a derived class object is assigned or passed by value to a base class variable. The derived parts of the object (extra data members and derived `vtable` pointer) are "sliced off", leaving only the base copy.

```cpp
class Animal { public: virtual void speak() { std::cout << "..."; } };
class Dog : public Animal { public: void speak() override { std::cout << "Woof"; } };

void listen(Animal a) { a.speak(); } // PASS BY VALUE -> Slicing occurs!

Dog d;
listen(d); // PRINTS "...", NOT "Woof"!
```

### Prevention
Always pass polymorphic objects by **const reference** (`const Animal&`) or **pointer** (`std::unique_ptr<Animal>`).

---

## 5. Construction / Destruction Ordering & Destructors

### Construction Order:
1. Virtual base classes (in depth-first left-to-right order).
2. Non-virtual base classes (left-to-right).
3. Class member variables in **declaration order inside the class body** (ignoring member initializer list order!).
4. Constructor body `{}` code.

### Destruction Order:
Strict reverse order of construction.

### Virtual Destructor Necessity
Deleting a derived object via a base class pointer (`Base* p = new Derived(); delete p;`) when `Base` lacks a `virtual` destructor is **Undefined Behavior**:
- Only `Base::~Base()` executes.
- `Derived::~Derived()` and derived member destructors (e.g. `std::vector`, `std::string`) are never called, causing massive memory/resource leaks.

---

## 6. Access Control & Inheritance Modes

| Inheritance Mode | Public Base Members | Protected Base Members | Private Base Members |
|---|---|---|---|
| `public` | `public` in derived | `protected` in derived | Inaccessible |
| `protected` | `protected` in derived | `protected` in derived | Inaccessible |
| `private` | `private` in derived | `private` in derived | Inaccessible |

- **`struct` vs `class`**: `struct` defaults to `public` member access and `public` inheritance. `class` defaults to `private` member access and `private` inheritance.
- **`explicit` Keyword**: Prevents implicit single-argument constructor conversions (`Widget w = 42;` fails if constructor is `explicit`).

---

## 7. Static Polymorphism (CRTP) vs Dynamic Polymorphism

### Curiously Recurring Template Pattern (CRTP)
CRTP achieves polymorphism at **compile time** with **zero runtime overhead**:

```cpp
template <typename Derived>
class Shape {
public:
    double area() const {
        return static_cast<const Derived*>(this)->area_impl();
    }
};

class Circle : public Shape<Circle> {
public:
    double area_impl() const { return 3.14159 * radius_ * radius_; }
private:
    double radius_ = 5.0;
};
```

| Feature | Dynamic Polymorphism (vtable) | Static Polymorphism (CRTP) |
|---|---|---|
| **Dispatch Timing** | Runtime indirect jump | Compile-time static call / inline |
| **Container Storage** | `std::vector<std::unique_ptr<Base>>` | Requires variant or template parameters |
| **Code Size** | Smaller binary | Template instantiation bloat |
| **Inlining** | Prevented (unless devirtualized) | Fully inlined by compiler |

---

## 8. Polymorphic Cloning & Covariant Return Types

Covariant return types allow an overriding method in a derived class to return a pointer/reference to a type derived from the return type of the base method:

```cpp
class Base {
public:
    virtual ~Base() = default;
    virtual std::unique_ptr<Base> clone() const = 0;
};

class Derived : public Base {
public:
    std::unique_ptr<Base> clone() const override {
        return std::make_unique<Derived>(*this); // Covariant memory allocation
    }
};
```
