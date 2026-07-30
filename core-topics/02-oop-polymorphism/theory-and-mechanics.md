# Theory & Mechanics: OOP & Polymorphism

## 1. VTable & VPtr — Memory Layout

Every class with at least one `virtual` function gets a **vtable** (one per class, stored in the read-only data segment). Every instance of that class gets a hidden **vptr** (typically 8 bytes on 64-bit) as its first member.

```
class Base {                         vtable for Base:
    virtual void foo();              ┌──────────────────────┐
    virtual void bar();              │ &Base::foo           │ slot 0
    int x;                           │ &Base::bar           │ slot 1
};                                   │ &Base::~Base         │ slot 2
                                     └──────────────────────┘

class Derived : public Base {        vtable for Derived:
    void foo() override;             ┌──────────────────────┐
    int y;                           │ &Derived::foo        │ slot 0 (overridden)
};                                   │ &Base::bar           │ slot 1 (inherited)
                                     │ &Derived::~Derived   │ slot 2
                                     └──────────────────────┘

Object layout — Derived d:
┌──────────────┐
│ vptr ─────────────→ Derived's vtable
├──────────────┤
│ Base::x      │
├──────────────┤
│ Derived::y   │
└──────────────┘
sizeof(Derived) = 8 (vptr) + 4 (x) + 4 (y) = 16 bytes
```

## 2. Dynamic Dispatch — Assembly Level

A virtual call `base_ptr->foo()` compiles to approximately:

```asm
; base_ptr in rdi
mov  rax, [rdi]           ; load vptr (first 8 bytes of object)
call [rax + 0]            ; call vtable[0] = foo's slot (indirect call)
```

This is an **indirect function call** — the CPU cannot predict the target at compile time. Cost:
- ~5 ns overhead vs direct call (indirect branch + possible **icache miss** if target code is cold)
- The `final` keyword enables **devirtualization** — the compiler can resolve the call statically

## 3. Virtual Destructors — Destruction Chain

When deleting through a base pointer, the vtable routes to the most-derived destructor, which then chains **upward**:

```cpp
Base* p = new Derived();
delete p;
// 1. vtable lookup → calls ~Derived()
// 2. ~Derived() body runs (destroys Derived members: y)
// 3. Compiler auto-calls ~Base() (destroys Base members: x)
// 4. operator delete(p) frees memory

// WITHOUT virtual destructor:
// delete p calls ~Base() directly — ~Derived() NEVER runs → resource leak + UB
```

**Rule:** If a class has ANY virtual function, its destructor MUST be virtual.

## 4. Diamond Problem & Virtual Inheritance

```
         Animal
        /      \
    Dog          Cat
        \      /
        DogCat (ambiguous!)
```

**Without virtual inheritance:** `DogCat` contains TWO copies of `Animal`:
```
DogCat layout:
┌──────────────────┐
│ Dog::vptr        │
│ Dog::Animal data │  ← first copy of Animal
│ Dog::data        │
├──────────────────┤
│ Cat::vptr        │
│ Cat::Animal data │  ← second copy of Animal (duplicate!)
│ Cat::data        │
└──────────────────┘
```

**With virtual inheritance** (`class Dog : virtual public Animal`):
```
DogCat layout:
┌──────────────────┐
│ Dog::vptr        │   (vtable includes vbase offset)
│ Dog::data        │
├──────────────────┤
│ Cat::vptr        │   (vtable includes vbase offset)
│ Cat::data        │
├──────────────────┤
│ Animal data      │  ← single shared copy (at end)
└──────────────────┘
```

The vtable stores a **vbase offset** — the distance from each sub-object to the shared `Animal` base. This adds one extra indirection for every access to the virtual base's members.

## 5. Object Slicing

```cpp
Derived d;
Base b = d;  // SLICING — only Base portion is copied

// b's vptr points to Base's vtable (NOT Derived's)
// b.foo() calls Base::foo, not Derived::foo
// Derived-specific data (y) is lost entirely
```

This is why polymorphism **requires pointers or references** — value semantics copy only the declared type's slice.

## 6. CRTP — Static Polymorphism (Zero Overhead)

```cpp
template <typename Derived>
class Base {
public:
    double compute(double x) const {
        // Compiler knows Derived at compile time → resolves statically → inlined
        return static_cast<const Derived*>(this)->compute_impl(x);
    }
};

class Fast : public Base<Fast> {
public:
    double compute_impl(double x) const { return x * 1.5; }
};
```

The `static_cast` is resolved at compile time — no vtable, no indirect call, no icache miss. The compiler inlines `compute_impl` directly. This is why HFT systems use CRTP for hot-path strategies.
