# Theory & Mechanics: Production C++ Idioms

## 1. The Pimpl Idiom (Pointer to Implementation)

Pimpl isolates binary interfaces (ABI) and speeds up compilation by hiding private members inside an opaque pointer to a forward-declared struct.

```cpp
// Widget.h
#include <memory>
class Widget {
public:
    Widget();
    ~Widget(); // MUST be declared in header, defined in .cpp where Impl is complete!
    void do_work();
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};
```

## 2. Type Erasure Mechanics

Type erasure combines template constructors with non-template virtual base interface wrappers to store any callable or value matching a specific interface without requiring public inheritance.

```
Type Erasure Object Layout:
+-------------------------------+
| unique_ptr<ConceptBase> vptr  | ----> [ Model<ConcreteType> ]
+-------------------------------+             |-- ConcreteType object
                                              |-- virtual void invoke()
```
