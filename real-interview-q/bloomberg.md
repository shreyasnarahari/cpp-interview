# Bloomberg — C++ Interview Questions

> **Focus:** RAII, const correctness, OOP

---

## Questions by Topic

### Memory Management & RAII

**Beginner**
- What is RAII? Why is it important in C++? **[G, BB]**

**Intermediate**
- What is `std::unique_ptr`? How does it differ from `std::shared_ptr`? **[G, M, BB]**
- Why should you use `std::make_unique` / `std::make_shared` instead of `new`? *(exception safety, single allocation for shared_ptr)* **[G, BB]**
- What is the overhead of `std::shared_ptr` compared to raw pointers? *(control block + atomic refcount)* **[JT, BB]**

**Advanced**
- How does `std::make_shared` differ from `std::shared_ptr<T>(new T)` in terms of memory layout? *(single allocation vs two)* **[G, BB]**

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
- Why must a base class destructor be virtual if used polymorphically? **[G, M, BB]**

**Intermediate**
- Explain the vtable mechanism. How does dynamic dispatch work? **[G, JT, BB]**
- What is the Rule of Three / Rule of Five / Rule of Zero? **[G, M, BB]**

**Advanced**
- How does `dynamic_cast` work internally? What does it need? *(RTTI, at least one virtual function)* **[G, BB]**

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

**Intermediate**
- What is SFINAE? Give an example. **[G, JT, BB]**

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

### Move Semantics & Value Categories

**Beginner**
- What is an lvalue? What is an rvalue? **[G, M, BB]**
- What does `std::move` do? *(it's just a cast to T&&, it doesn't move anything)* **[G, M, BB]**

**Intermediate**
- What is the state of a moved-from object? *(valid but unspecified)* **[G, BB]**
- What is copy elision? What is RVO vs NRVO? **[G, JT, BB]**
- Why should move operations be `noexcept`? *(std::vector only uses move if noexcept)* **[G, BB, JT]**

**Gotcha Code**
```cpp
// Q: What does this print?
class Widget {
public:
    Widget() { std::cout << "default\n"; }
    Widget(const Widget&) { std::cout << "copy\n"; }
    Widget(Widget&&) { std::cout << "move\n"; }
};

Widget createWidget() {
    Widget w;
    return w;  // NRVO candidate
}

int main() {
    Widget w = createWidget();
}
// A: Most likely "default" only (NRVO elides the move/copy).
// Without NRVO: "default" then "move". Copy is never used here.
```

```cpp
// Q: Is this correct?
void process(std::string&& s) {
    std::string local = s;  // copy or move?
}
// A: COPY! Even though s is declared as &&, inside the function s is
// an lvalue (it has a name). You must std::move(s) to trigger move.
// Fix: std::string local = std::move(s);
```

```cpp
// Q: What's wrong with this code?
std::unique_ptr<Widget> create() {
    auto w = std::make_unique<Widget>();
    return std::move(w);  // correct?
}
// A: It works, but std::move is HARMFUL here — it prevents NRVO.
// Fix: just return w; — the compiler will apply NRVO or implicit move.
```

### STL Containers & Iterators

**Beginner**
- What is `push_back` vs `emplace_back`? **[G, M, BB]**

**Intermediate**
- Explain iterator invalidation rules for `std::vector`. **[G, M, BB]**
- What happens to iterators when you insert into a `std::map`? *(they remain valid)* **[G, BB]**

**Advanced**
- What is the Small Buffer Optimization in `std::string`? **[JT, BB]**

**Gotcha Code**
```cpp
// Q: What's wrong with this code?
std::vector<int> v = {1, 2, 3, 4, 5};
for (auto it = v.begin(); it != v.end(); ++it) {
    if (*it % 2 == 0) {
        v.erase(it);  // ?
    }
}
// A: Undefined behavior — erase invalidates the iterator `it`.
// After erase, `it` is dangling, and ++it is UB.
// Fix: it = v.erase(it); (erase returns next valid iterator)
// and don't ++it in the for when erasing.
```

```cpp
// Q: Is this safe?
std::vector<int> v = {1, 2, 3};
int& ref = v[0];
v.push_back(4);
std::cout << ref;  // ?
// A: Undefined behavior — push_back may trigger reallocation,
// which invalidates ALL references, pointers, and iterators.
// After reallocation, ref points to freed memory.
```

```cpp
// Q: What does this print?
std::map<int, std::string> m;
std::cout << m[42];  // ?
// A: Prints "" (empty string). operator[] inserts a default-constructed
// value if the key doesn't exist. m now has size 1 with key 42.
// This is why operator[] can't be used on const maps.
// Use m.at(42) to throw, or m.find(42) to check safely.
```

### Concurrency & Multithreading

**Beginner**
- What is a mutex? Why do you need it? **[G, M, BB]**

**Intermediate**
- What is the difference between `std::lock_guard` and `std::unique_lock`? **[G, BB]**
- What is a condition variable? What are spurious wakeups? **[G, M, BB]**
- What is a deadlock? How do you prevent it? **[G, M, AM, BB]**
- What is `std::atomic`? When would you use it instead of a mutex? **[G, JT, BB]**
- What is `std::scoped_lock` (C++17) and why is it preferred over multiple `lock_guard`? **[G, BB]**

**Advanced**
- Implement a thread-safe singleton. **[G, M, BB]**

**Gotcha Code**
```cpp
// Q: What's wrong with this code?
std::mutex mtx;
int counter = 0;

void increment() {
    mtx.lock();
    counter++;
    if (counter > 100) {
        return;  // BUG!
    }
    mtx.unlock();
}
// A: If counter > 100, the function returns without unlocking the mutex.
// Next call to lock() will deadlock (or throw with recursive_mutex).
// Fix: use std::lock_guard<std::mutex> lock(mtx); — RAII guarantees unlock.
```

```cpp
// Q: Is this thread-safe?
bool ready = false;

void producer() {
    // prepare data...
    ready = true;  // signal consumer
}

void consumer() {
    while (!ready) {}  // spin-wait
    // use data...
}
// A: NO — data race on `ready`. Both threads access a non-atomic bool
// without synchronization. The compiler may optimize away the loop
// (ready never changes from consumer's perspective).
// Fix: std::atomic<bool> ready{false};
```

```cpp
// Q: Does this avoid deadlock?
std::mutex m1, m2;

void threadA() {
    std::lock_guard<std::mutex> l1(m1);
    std::lock_guard<std::mutex> l2(m2);
    // work...
}

void threadB() {
    std::lock_guard<std::mutex> l1(m2);  // opposite order!
    std::lock_guard<std::mutex> l2(m1);
    // work...
}
// A: NO — classic deadlock. threadA holds m1, waits for m2.
// threadB holds m2, waits for m1. Circular wait.
// Fix: std::scoped_lock lock(m1, m2); — uses deadlock avoidance algorithm.
```

### Lambda Expressions

**Intermediate**
- What is a lambda capture? Explain [=], [&], [x], [&x], [this], [*this]. **[G, M, BB]**

**Advanced**
- What is an immediately invoked lambda expression (IILE)? When is it useful? **[G, BB]**
- What is the lifetime issue with lambda captures by reference? **[G, M, BB]**

**Gotcha Code**
```cpp
// Q: What does this print?
int x = 10;
auto fn = [x]() mutable {
    x += 5;
    std::cout << x << " ";
};
fn();
fn();
std::cout << x;
// A: "15 20 10" — the lambda captures x BY VALUE. The mutable keyword
// lets it modify its own copy. The original x is never changed.
// Each call modifies the lambda's internal copy, which persists between calls.
```

```cpp
// Q: What's the problem?
std::function<void()> createTimer() {
    std::string message = "timeout!";
    return [&message]() {
        std::cout << message;  // ?
    };
}
// A: Dangling reference — message is destroyed when createTimer returns.
// The lambda captures it by reference, so it's a dangling reference.
// Fix: capture by value [message] or [msg = std::move(message)]
```

```cpp
// Q: Does this compile?
auto fn = [](int a, int b) { return a + b; };
fn = [](int a, int b) { return a * b; };  // ?
// A: NO — each lambda has a unique type. You can't reassign a lambda
// variable to a different lambda. Fix: use std::function<int(int,int)>.
```

### Const Correctness & Type Casting

**Beginner**
- What is a const member function? **[G, BB]**

**Intermediate**
- What is the `mutable` keyword? Give a real use case. *(caching, mutex in const method)* **[G, BB, JT]**
- When would you use `const_cast`? Is it ever safe? *(only if the original object is non-const)* **[G, BB]**
- What is the difference between `constexpr` and `const`? **[G, M, BB]**

**Advanced**
- Why can't you call a non-const method on a const object? Explain the const overloading pattern. **[G, BB]**

**Gotcha Code**
```cpp
// Q: Does this compile?
const int ci = 42;
int* p = const_cast<int*>(&ci);
*p = 100;
std::cout << ci << " " << *p;
// A: Compiles, but *p = 100 is UNDEFINED BEHAVIOR because ci is
// truly const (defined as const). The compiler may have inlined 42
// everywhere ci appears. Output is unpredictable.
// const_cast is only safe when the original object is actually non-const.
```

```cpp
// Q: What is printed?
class Cache {
    mutable int hits = 0;
public:
    int get() const {
        hits++;  // OK — mutable member
        return 42;
    }
    int getHits() const { return hits; }
};

const Cache c;
c.get();
c.get();
std::cout << c.getHits();
// A: Prints 2. mutable allows modification even on const objects.
// Common use case: caching, logging, mutex locking in const methods.
```

---

## Company-Specific Focus Questions

1. What is RAII? Implement a file handle wrapper
2. Explain const correctness — what is a const member function?
3. What is the Rule of Three? Write copy constructor and assignment operator
4. What is the `mutable` keyword? Give a real use case
5. Explain `std::shared_ptr` and `std::weak_ptr` — implement observer pattern


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
