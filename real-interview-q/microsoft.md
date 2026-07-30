# Microsoft — C++ Interview Questions

> **Focus:** OOP & systems

---

## Questions by Topic

### Memory Management & RAII

**Beginner**
- What does `new` return? What does `delete` do? **[AM, MS]**

**Intermediate**
- What happens if you mix `new[]` with `delete` (not `delete[]`)? *(undefined behavior)* **[MS, AM]**

**Gotcha Code**
```cpp
// Q: What's wrong with this code?
void process() {
    int* p = new int[100];
    doWork(p);   // might throw
    delete p;    // wrong delete AND leak if doWork throws
}
// A: Two bugs: (1) new[] must use delete[], not delete — UB.
// (2) If doWork() throws, delete is never reached → memory leak.
// Fix: use std::vector<int> or std::unique_ptr<int[]>.
```

```cpp
// Q: Is this safe?
std::shared_ptr<Widget> sp1(new Widget);
std::shared_ptr<Widget> sp2(sp1.get()); // ?
// A: NO — double delete! sp2 creates a separate control block for the
// same raw pointer. When both go out of scope, Widget is deleted twice → UB.
// Fix: copy the shared_ptr: auto sp2 = sp1;
```

```cpp
// Q: What does this print?
struct Node {
    std::shared_ptr<Node> next;
    ~Node() { std::cout << "destroyed\n"; }
};

void leak() {
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->next = b;
    b->next = a;  // circular reference
}
// A: Prints nothing — neither Node is ever destroyed.
// The circular shared_ptr references keep refcounts at 1 forever.
// Fix: make one of the pointers a std::weak_ptr.
```

### OOP & Polymorphism

**Beginner**
- What is the difference between `struct` and `class` in C++? *(default access: public vs private)* **[G, AM, MS]**
- What is a pure virtual function? What is an abstract class? **[AM, MS]**

**Intermediate**
- What is the diamond problem? How does virtual inheritance solve it? **[G, MS]**

**Gotcha Code**
```cpp
// Q: What does this print?
class Base {
public:
    Base() { init(); }
    virtual void init() { std::cout << "Base::init\n"; }
};

class Derived : public Base {
public:
    void init() override { std::cout << "Derived::init\n"; }
};

int main() {
    Derived d;
}
// A: Prints "Base::init" — NOT "Derived::init".
// During Base construction, the vtable points to Base's methods.
// Virtual dispatch does NOT work in constructors/destructors.
```

```cpp
// Q: What's wrong here?
class Base {
public:
    ~Base() { std::cout << "Base destroyed\n"; }
};

class Derived : public Base {
    int* data;
public:
    Derived() : data(new int[100]) {}
    ~Derived() { delete[] data; }
};

void f() {
    Base* p = new Derived();
    delete p;  // ?
}
// A: Only ~Base() is called — ~Derived() never runs → memory leak.
// The destructor is not virtual, so delete through Base* calls ~Base().
// Fix: virtual ~Base() { ... }
```

```cpp
// Q: Does this compile?
class Animal {
public:
    virtual void speak() = 0;
};

class Dog : public Animal {
public:
    void speak() { std::cout << "Woof\n"; }
};

int main() {
    Animal a;  // ?
}
// A: Compile error — Animal is abstract (has pure virtual function).
// Cannot instantiate abstract classes. Fix: Animal* a = new Dog();
```

### Templates

**Beginner**
- What is the difference between a template and a macro? **[MS, AM]**

**Gotcha Code**
```cpp
// Q: Does this compile? If so, what does it print?
template <typename T>
void print(T val) { std::cout << "generic: " << val << "\n"; }

template <>
void print<int>(int val) { std::cout << "int: " << val << "\n"; }

void print(int val) { std::cout << "non-template: " << val << "\n"; }

int main() {
    print(42);
    print<int>(42);
    print(3.14);
}
// A: "non-template: 42" — non-template wins over template for exact match
//    "int: 42" — explicit <int> forces template, specialization is picked
//    "generic: 3.14" — only the generic template matches double
```

```cpp
// Q: Why does this fail to compile?
template <typename T>
class Stack {
    std::vector<T> data;
public:
    void push(const T& val) { data.push_back(val); }
    T pop() {
        T top = data.back();
        data.pop_back();
        return top;
    }
};

template <typename T>
class Stack<T*> {  // Partial specialization
    // ...
};

template <>
void Stack<int>::push(const int& val) { /* ... */ } // Error?
// A: You cannot specialize a member function of a class template
// without specializing the entire class. You'd need to fully
// specialize Stack<int> first.
```

---

## Company-Specific Focus Questions

1. Explain multiple inheritance and the diamond problem
2. What is virtual inheritance? What is its overhead?
3. What is the difference between `struct` and `class`?
4. Explain how `dynamic_cast` works
5. What are the C++ type casting operators? When to use each?


---

## Master Gotcha Table

| Gotcha | The Trap | The Fix |
|--------|----------|---------|
| Virtual dispatch in constructor | vtable points to base during construction | Never call virtual functions in ctor/dtor |
| Non-virtual destructor | Derived destructor not called through base ptr | Always make base dtor virtual |
| Object slicing | Derived copied into base by value | Use pointers/references for polymorphism |
| Dangling reference | Returning reference to local variable | Return by value or use heap |
| Iterator invalidation | Using iterator after container modification | Reassign: `it = container.erase(it)` |
| shared_ptr from raw pointer | Two control blocks for same object | Copy the shared_ptr, or use enable_shared_from_this |
| std::move doesn't move | It just casts to T&&; you still need move ctor | Ensure move operations are defined |
| Signed integer overflow | UB — compiler assumes it doesn't happen | Use unsigned, or check before operation |
| new[] with delete | Must use delete[] for arrays | Prefer std::vector or unique_ptr<T[]> |
| Lambda capture by ref | Reference may outlive captured variable | Capture by value, or ensure lifetime |
| const_cast on true const | Modifying truly const object is UB | Only const_cast away const if original is non-const |
| Missing noexcept on move | std::vector copies instead of moves during realloc | Mark move ctor/assignment as noexcept |
