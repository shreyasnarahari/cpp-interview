> Organized by topic, difficulty, and company where data is available.
> Focus: real questions that appear in actual C++ interviews.

---

## Company Focus Legend
- **G** = Google
- **M** = Meta (Facebook)
- **MS** = Microsoft
- **AM** = Amazon
- **AP** = Apple
- **BB** = Bloomberg
- **JT** = Jane Street / Two Sigma (Quant firms)
- **CT** = Citadel / Citadel Securities

---

## 80/20 Priority Guide
| Priority | Topics | Coverage |
|----------|--------|----------|
| P1 — Do first | Memory Management & RAII, OOP & Polymorphism, Templates, Move Semantics | ~60% of all C++ interviews |
| P2 — Week 2 | STL Containers, Concurrency, Lambdas, Const Correctness, UB & Pitfalls | ~20% additional |
| P3 — Senior level | Memory model, Template metaprogramming, Cache optimization, Design patterns, Coroutines | Remaining 20% |

---

## 1. Memory Management & RAII

### Beginner
- What is the difference between stack and heap allocation? **[G, M, AM]**
- What does `new` return? What does `delete` do? **[AM, MS]**
- What is RAII? Why is it important in C++? **[G, BB]**
- What happens if you call `delete` on a null pointer? *(nothing — it's a no-op)*

### Intermediate
- What is `std::unique_ptr`? How does it differ from `std::shared_ptr`? **[G, M, BB]**
- When would you use `std::weak_ptr`? Give a real example. *(breaking circular references)* **[G, M]**
- Why should you use `std::make_unique` / `std::make_shared` instead of `new`? *(exception safety, single allocation for shared_ptr)* **[G, BB]**
- What is the overhead of `std::shared_ptr` compared to raw pointers? *(control block + atomic refcount)* **[JT, BB]**
- What happens if you mix `new[]` with `delete` (not `delete[]`)? *(undefined behavior)* **[MS, AM]**

### Advanced
- Explain the control block of `std::shared_ptr`. What does it contain? **[G, JT]**
- What is placement new? When would you use it? **[JT, G]**
- How does `std::make_shared` differ from `std::shared_ptr<T>(new T)` in terms of memory layout? *(single allocation vs two)* **[G, BB]**
- What is a custom deleter? Write a `unique_ptr` that manages a `FILE*`. **[G, M]**

### Gotcha Code
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

---

## 2. OOP & Polymorphism

### Beginner
- What is the difference between `struct` and `class` in C++? *(default access: public vs private)* **[G, AM, MS]**
- What is a virtual function? Why do we need it? **[G, M, AM]**
- What is a pure virtual function? What is an abstract class? **[AM, MS]**
- Why must a base class destructor be virtual if used polymorphically? **[G, M, BB]**

### Intermediate
- Explain the vtable mechanism. How does dynamic dispatch work? **[G, JT, BB]**
- What is object slicing? When does it happen? **[G, M]**
- What is the Rule of Three / Rule of Five / Rule of Zero? **[G, M, BB]**
- What is the diamond problem? How does virtual inheritance solve it? **[G, MS]**
- What is the difference between `override` and `final`? **[M, AM]**
- Explain CRTP (Curiously Recurring Template Pattern). When would you use it? **[G, JT]**

### Advanced
- What is the overhead of a virtual function call compared to a direct call? **[JT, G]**
- How does `dynamic_cast` work internally? What does it need? *(RTTI, at least one virtual function)* **[G, BB]**
- Explain the memory layout of an object with multiple inheritance and virtual functions. **[JT, G]**
- What is the Empty Base Optimization (EBO)? **[JT]**

### Gotcha Code
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

---

## 3. Templates

### Beginner
- What is a function template? What is a class template? **[G, M]**
- What is template argument deduction? **[G]**
- What is the difference between a template and a macro? **[MS, AM]**

### Intermediate
- What is template specialization? What is partial specialization? **[G, JT]**
- What is SFINAE? Give an example. **[G, JT, BB]**
- What are variadic templates? What is a parameter pack? **[G, M]**
- What are fold expressions (C++17)? **[G, JT]**
- What is `if constexpr` (C++17) and how does it differ from regular `if`? **[G, M]**

### Advanced
- What are C++20 Concepts? How do they improve on SFINAE? **[G, JT]**
- Explain template metaprogramming. Write compile-time factorial. **[JT, G]**
- What is tag dispatch? When would you use it? **[JT]**
- What is `std::void_t` and how is it used for detection idiom? **[JT, G]**

### Gotcha Code
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

## 4. Move Semantics & Value Categories

### Beginner
- What is an lvalue? What is an rvalue? **[G, M, BB]**
- What does `std::move` do? *(it's just a cast to T&&, it doesn't move anything)* **[G, M, BB]**
- What is a move constructor? When is it called? **[G, M, AM]**

### Intermediate
- What is the state of a moved-from object? *(valid but unspecified)* **[G, BB]**
- When is the move constructor implicitly generated? When is it deleted? **[G, M]**
- What is copy elision? What is RVO vs NRVO? **[G, JT, BB]**
- Why should move operations be `noexcept`? *(std::vector only uses move if noexcept)* **[G, BB, JT]**
- What is the difference between a forwarding reference (`T&&` in template) and an rvalue reference? **[G, JT]**

### Advanced
- Explain reference collapsing rules. **[JT, G]**
- What is perfect forwarding? Write a `make_unique` implementation. **[G, JT]**
- When does `std::move` on a return value PREVENT optimization? *(defeats NRVO)* **[G, JT]**
- What are the C++17 guaranteed copy elision rules? **[JT, G]**

### Gotcha Code
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

---

## 5. STL Containers & Iterators

### Beginner
- When would you use `std::vector` vs `std::list`? **[G, M, AM]**
- What is the time complexity of `std::map::find()` vs `std::unordered_map::find()`? **[G, AM]**
- What is `push_back` vs `emplace_back`? **[G, M, BB]**

### Intermediate
- Explain iterator invalidation rules for `std::vector`. **[G, M, BB]**
- What happens to iterators when you insert into a `std::map`? *(they remain valid)* **[G, BB]**
- What is the difference between `std::map` and `std::unordered_map` internally? *(red-black tree vs hash table)* **[G, M, AM]**
- When is `std::map` faster than `std::unordered_map`? *(small n, bad hash, need ordering)* **[JT, G]**
- What is `std::vector::reserve()` vs `std::vector::resize()`? **[G, M]**

### Advanced
- What is `std::span` (C++20)? How does it differ from passing vector by reference? **[G]**
- What is the Small Buffer Optimization in `std::string`? **[JT, BB]**
- How does `std::vector` grow? What is the growth factor? *(typically 1.5x or 2x, implementation-defined)* **[JT, G]**
- What is `std::deque`'s internal structure? *(array of fixed-size blocks)* **[JT]**

### Gotcha Code
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

---

## 6. Concurrency & Multithreading

### Beginner
- How do you create a thread in C++? **[G, M, AM]**
- What is a mutex? Why do you need it? **[G, M, BB]**
- What is `std::lock_guard`? How does it relate to RAII? **[G, M]**

### Intermediate
- What is the difference between `std::lock_guard` and `std::unique_lock`? **[G, BB]**
- What is a condition variable? What are spurious wakeups? **[G, M, BB]**
- What is a deadlock? How do you prevent it? **[G, M, AM, BB]**
- What is `std::atomic`? When would you use it instead of a mutex? **[G, JT, BB]**
- What is `std::async`? What is the difference between `std::launch::async` and `std::launch::deferred`? **[M, AM]**
- What is `std::scoped_lock` (C++17) and why is it preferred over multiple `lock_guard`? **[G, BB]**

### Advanced
- Explain the C++ memory model. What are memory orderings (relaxed, acquire, release, seq_cst)? **[JT, G]**
- What is a data race vs a race condition? **[G, JT]**
- What is `std::memory_order_acquire` / `std::memory_order_release`? When would you use them? **[JT]**
- What is false sharing? How do you avoid it? *(cache line padding, alignas)* **[JT, G]**
- Implement a thread-safe singleton. **[G, M, BB]**

### Gotcha Code
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

---

## 7. Lambda Expressions

### Intermediate
- What is a lambda capture? Explain [=], [&], [x], [&x], [this], [*this]. **[G, M, BB]**
- What is a mutable lambda? When do you need it? **[G, M]**
- What is the difference between a lambda and `std::function`? *(lambda is a unique unnamed type; std::function is type-erased with overhead)* **[G, JT]**
- What is a generic lambda (C++14)? How does it work internally? *(templated operator())* **[G, M]**

### Advanced
- What is an init capture (C++14)? How do you move a `unique_ptr` into a lambda? **[G, M]**
- What is an immediately invoked lambda expression (IILE)? When is it useful? **[G, BB]**
- What is a template lambda (C++20)? How does it differ from a generic lambda? **[JT, G]**
- What is the lifetime issue with lambda captures by reference? **[G, M, BB]**

### Gotcha Code
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

---

## 8. Const Correctness & Type Casting

### Beginner
- What is the difference between `const int*`, `int* const`, and `const int* const`? **[G, M, AM]**
- What is a const member function? **[G, BB]**

### Intermediate
- What is the `mutable` keyword? Give a real use case. *(caching, mutex in const method)* **[G, BB, JT]**
- What is the difference between `static_cast` and `dynamic_cast`? **[G, M, AM]**
- When would you use `const_cast`? Is it ever safe? *(only if the original object is non-const)* **[G, BB]**
- What is `reinterpret_cast`? When is it needed? *(low-level, serialization, hardware registers)* **[JT, G]**
- What is the difference between `constexpr` and `const`? **[G, M, BB]**

### Advanced
- What are `consteval` and `constinit` (C++20)? **[JT, G]**
- Why can't you call a non-const method on a const object? Explain the const overloading pattern. **[G, BB]**

### Gotcha Code
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

## 9. Undefined Behavior & Common Pitfalls

> The #1 topic that separates senior C++ engineers from everyone else.

### Intermediate
- What is undefined behavior? Give 5 examples. **[G, M, JT]**
- What is the difference between undefined behavior, unspecified behavior, and implementation-defined behavior? **[G, JT]**
- Is signed integer overflow defined in C++? *(no — UB)* **[JT, G]**
- What tools detect UB at runtime? *(ASan, UBSan, MSan, TSan)* **[G, M]**

### Advanced
- What is strict aliasing? When does it matter? **[JT, G]**
- What is sequence point violation? Give an example. **[JT]**
- Can the compiler assume UB never happens? *(yes — "time travel" optimization)* **[JT, G]**

### Gotcha Code
```cpp
// Q: What does this print?
int i = 0;
std::cout << i++ << " " << i++ << " " << i++;
// A: Undefined behavior before C++17 (unsequenced modifications).
// In C++17: the order of operand evaluation for << is left-to-right,
// so it prints "0 1 2". But avoid writing code that depends on this.
```

```cpp
// Q: What happens?
int arr[5] = {1, 2, 3, 4, 5};
std::cout << arr[5];
// A: Undefined behavior — out-of-bounds access. arr[5] reads past
// the array. No bounds checking in raw arrays. May crash, return
// garbage, or appear to work (worst case — silent corruption).
// Fix: use std::array<int,5> with .at(5) which throws std::out_of_range.
```

```cpp
// Q: Is this safe?
int* dangling() {
    int x = 42;
    return &x;  // returning address of local
}
int* p = dangling();
std::cout << *p;
// A: Undefined behavior — x is destroyed when dangling() returns.
// p is a dangling pointer. May print 42 (memory not yet overwritten)
// or crash or anything. Fix: return by value, use smart pointer, or heap.
```

```cpp
// Q: What's wrong?
int x = INT_MAX;
x = x + 1;  // ?
// A: Signed integer overflow is UNDEFINED BEHAVIOR in C++.
// The compiler may assume this never happens and optimize accordingly.
// For unsigned: wraps around (defined). For signed: UB.
```

---

## Company-Specific Focus Areas

### Google (depth on language mechanics & design)
1. Explain the vtable mechanism — how does virtual dispatch work?
2. Write a `make_unique` implementation using perfect forwarding
3. What is SFINAE? Write a function that only accepts integral types
4. Explain move semantics — what does std::move actually do?
5. What is the Rule of Five? When does Rule of Zero apply?
6. Implement a thread-safe queue using condition variables
7. What is copy elision? When is it mandatory (C++17)?

### Meta (concurrency & production systems)
1. How do you prevent deadlocks in a system with many mutexes?
2. Explain `std::shared_ptr` thread safety — what is and isn't thread-safe?
3. What is a data race? How do you detect it? (TSan)
4. Design a lock-free queue (advanced)
5. When would you use `std::atomic` vs `std::mutex`?
6. Explain the producer-consumer pattern in C++

### Microsoft (OOP & systems)
1. Explain multiple inheritance and the diamond problem
2. What is virtual inheritance? What is its overhead?
3. What is the difference between `struct` and `class`?
4. Explain how `dynamic_cast` works
5. What are the C++ type casting operators? When to use each?

### Amazon (practical coding & correctness)
1. Reverse a linked list — discuss ownership semantics
2. Implement an LRU cache — discuss container choice (unordered_map + list)
3. What is iterator invalidation? Give examples for vector, map
4. What is the difference between `map` and `unordered_map`?
5. What is exception safety? Explain the three guarantee levels

### Bloomberg (RAII, const correctness, OOP)
1. What is RAII? Implement a file handle wrapper
2. Explain const correctness — what is a const member function?
3. What is the Rule of Three? Write copy constructor and assignment operator
4. What is the `mutable` keyword? Give a real use case
5. Explain `std::shared_ptr` and `std::weak_ptr` — implement observer pattern

### Citadel / Citadel Securities (low-latency, lock-free, hardware sympathy)
> See also: [citadel-prep.md](../citadel-prep.md) for full implementations and system design

**Lock-Free & Atomics**
1. Implement a lock-free SPSC (single-producer, single-consumer) queue **[Critical]**
2. Explain `memory_order_acquire` vs `memory_order_release` — when is each used? **[Critical]**
3. What is the cost of `seq_cst` vs `acquire/release` on x86? On ARM? **[Critical]**
4. Implement a spinlock using `std::atomic` — what memory ordering do you use?
5. What is the ABA problem? How do tagged pointers solve it?
6. What is false sharing? How do you detect and fix it? (`alignas(64)`, perf stat)
7. When would you use `compare_exchange_weak` vs `compare_exchange_strong`?

**Memory & Allocators**
8. Implement an arena (bump) allocator — explain the trade-offs
9. Implement a fixed-size object pool using a free list
10. What happens when you call `new`? Trace from C++ → malloc → kernel
11. What is placement new? When do you need explicit destructor calls?
12. Why are allocations non-deterministic? What is `mmap` vs `brk`?

**Performance & Cache**
13. Explain the CPU cache hierarchy (L1/L2/L3). What are the latency numbers?
14. What is cache-line size? How does it affect data structure design?
15. SoA vs AoS — when does each perform better?
16. What is branch prediction? How do you write branchless code?
17. Why is `std::vector` faster than `std::list` for traversal despite O(n) insert?
18. What is `[[likely]]` / `[[unlikely]]` (C++20)?

**System Design**
19. Design an in-memory limit order book with price-time priority
20. Design an SPSC market data pipeline — from network to strategy
21. How would you handle 10 million messages/second with < 1µs latency?

**Templates & Compile-Time**
22. How do you use CRTP to avoid virtual function overhead?
23. Write a compile-time lookup table using `constexpr`
24. When is `consteval` (C++20) better than `constexpr`?

### Jane Street / Two Sigma / Other Quant Firms (low-level, performance, templates)
1. What is cache locality? How does it affect container choice?
2. Explain memory orderings in `std::atomic`
3. What is template metaprogramming? Implement compile-time Fibonacci
4. What is false sharing? How do you avoid it?
5. What is strict aliasing? When does it matter?
6. What is placement new? Implement a simple memory pool
7. Explain the C++ memory model — what is happens-before?

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
