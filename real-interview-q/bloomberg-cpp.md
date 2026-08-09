# Bloomberg — C++ Language & Mechanics Questions

> **Focus:** RAII, const correctness, OOP, thread safety, memory management, STL internals, template metaprogramming, assembly mechanics
>
> This file covers **pure C++ language mechanics** — the questions that test whether you truly understand how the language works under the hood. For system/data-structure design questions, see [bloomberg-design.md](./bloomberg-design.md).

---

## Questions by Topic

### Memory Management & RAII

**Beginner**

**Q: What is RAII? Why is it important in C++?**

RAII (Resource Acquisition Is Initialization) binds a resource's lifetime to an object's scope. When the object is constructed, the resource is acquired. When the object is destroyed (goes out of scope, stack unwinds), the resource is released — **automatically and deterministically**.

```cpp
void process_data() {
    std::lock_guard<std::mutex> lock(mtx);     // mutex acquired
    auto file = std::make_unique<File>("data"); // file opened
    auto conn = std::make_unique<DBConn>();     // connection opened
    
    do_work();  // if this throws, stack unwinds:
    //   ~conn → connection closed
    //   ~file → file closed
    //   ~lock → mutex released
    // NO resource leaks, even with exceptions!
}
```

Why it matters at Bloomberg: Bloomberg's Terminal processes millions of financial data events per second. Without RAII, any exception or early return in a data handler would leak connections, file handles, or mutexes — causing cascading failures. RAII makes resource safety **automatic and non-negotiable**.

---

**Intermediate**

**Q: What is `std::unique_ptr`? How does it differ from `std::shared_ptr`?**

| Property | `unique_ptr` | `shared_ptr` |
|---|---|---|
| Ownership | Exclusive — only one owner at a time | Shared — multiple owners via reference counting |
| Copy | Not copyable (compile error) | Copyable (increments refcount atomically) |
| Move | Moveable — transfers ownership | Moveable — transfers without refcount change |
| Overhead | Zero overhead vs raw pointer | Control block + atomic refcount (~16-32 bytes) |
| Thread safety | Not thread-safe (single owner) | Refcount operations are thread-safe; pointed-to object is NOT |
| Custom deleter | Part of the type (`unique_ptr<T, Deleter>`) | Type-erased (stored in control block) |

```cpp
// unique_ptr — exclusive ownership
auto widget = std::make_unique<Widget>();
// auto copy = widget;                // ERROR — can't copy
auto moved = std::move(widget);       // OK — ownership transferred
// widget is now nullptr

// shared_ptr — shared ownership
auto sp1 = std::make_shared<Widget>();
auto sp2 = sp1;                       // OK — refcount = 2
// Widget destroyed when both sp1 and sp2 go out of scope
```

**Bloomberg rule of thumb:** Default to `unique_ptr`. Use `shared_ptr` only when ownership is genuinely shared (e.g., multiple systems hold references to the same market data snapshot).

---

**Q: Why should you use `std::make_unique` / `std::make_shared` instead of `new`?**

**Reason 1 — Exception safety:**
```cpp
// DANGEROUS — potential memory leak:
process(std::shared_ptr<A>(new A), std::shared_ptr<B>(new B));
// If new A succeeds, then new B throws → A is leaked!
// Evaluation order of function arguments is unspecified prior to C++17.

// SAFE — make_shared is a single expression:
process(std::make_shared<A>(), std::make_shared<B>());
// Each make_shared is atomic — no interleaving possible.
```

**Reason 2 — `make_shared` uses single allocation:**
```cpp
// Two allocations:
std::shared_ptr<T> sp(new T);        // alloc 1: T on heap, alloc 2: control block

// Single allocation (T embedded in control block):
auto sp = std::make_shared<T>();     // alloc 1: control block + T together
// Fewer syscalls, better cache locality
```

**Trade-off:** With `make_shared`, the `T` memory is not freed until all `weak_ptr`s are gone (because `T` shares memory with the control block).

---

**Q: What is the overhead of `std::shared_ptr` compared to raw pointers?**

```
Raw pointer:  sizeof(T*) = 8 bytes, 0 overhead on copy/destroy

shared_ptr:   sizeof = 16 bytes (2 pointers: T* + control_block*)
              Control block: ~32-48 bytes (strong_count, weak_count, deleter, allocator)
              Copy:   atomic fetch_add(1, relaxed)     ~5-10 ns
              Destroy: atomic fetch_sub(1, acq_rel)    ~5-10 ns + potential deallocation
              
unique_ptr:   sizeof = 8 bytes (same as raw pointer, if deleter is stateless)
              Copy:   COMPILE ERROR
              Move:   pointer swap — 0 overhead
```

In hot paths (Bloomberg ticker processing), `shared_ptr` copies can become a measurable bottleneck due to atomic refcount operations (`lock xadd`) on every copy/destruction.

---

**Advanced**

**Q: How does `std::make_shared` differ from `std::shared_ptr<T>(new T)` in terms of memory layout?**

```
shared_ptr<T>(new T) — TWO allocations:

  shared_ptr:           Control Block:           T object:
  ┌──────────────┐      ┌──────────────────┐     ┌──────────┐
  │ T* ptr ──────────────────────────────────────→│ T data   │
  │ CB* ctrl ────────→  │ strong_count: 1  │     └──────────┘
  └──────────────┘      │ weak_count:   1  │
                        │ deleter          │
                        └──────────────────┘

make_shared<T>() — SINGLE allocation:

  shared_ptr:           Control Block + T (one block):
  ┌──────────────┐      ┌──────────────────┐
  │ T* ptr ──────────→  │ strong_count: 1  │
  │ CB* ctrl ────────→  │ weak_count:   1  │
  └──────────────┘      │ deleter          │
                        │ T data (in-place)│
                        └──────────────────┘
```

---

**Q: Explain `enable_shared_from_this` — how does it prevent double deletion, and why does calling `shared_from_this()` inside a constructor crash?**

`enable_shared_from_this<T>` adds an internal private `weak_ptr<T>` to the class. When the object is first wrapped in a `shared_ptr`, the `shared_ptr` constructor detects this base class and initializes the internal `weak_ptr`. Calling `shared_from_this()` then promotes that `weak_ptr` to a `shared_ptr`, **reusing the existing control block** instead of creating a duplicate.

```cpp
class Widget : public std::enable_shared_from_this<Widget> {
public:
    Widget() {
        // CRASH / BAD_WEAK_PTR TRAP:
        // auto self = shared_from_this(); // THROWS std::bad_weak_ptr!
        // Reason: The shared_ptr control block has NOT been constructed yet during Widget's constructor!
    }

    std::shared_ptr<Widget> get_self() {
        return shared_from_this();  // SAFE — reuses existing control block
        // return std::shared_ptr<Widget>(this);  // BUG — double delete!
    }
};

auto sp1 = std::make_shared<Widget>();
auto sp2 = sp1->get_self();  // sp1 and sp2 share the SAME control block
```

---

**Q: How do custom allocators work in C++? Implement a C++11 compliant `allocator<T>` template.**

Custom allocators decouple memory allocation from object construction. A standard allocator must provide `value_type`, `allocate(n)`, and `deallocate(p, n)`:

```cpp
#include <cstddef>
#include <new>
#include <utility>
#include <iostream>

template <typename T>
struct CustomAllocator {
    using value_type = T;

    CustomAllocator() noexcept = default;

    template <typename U>
    constexpr CustomAllocator(const CustomAllocator<U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > static_cast<std::size_t>(-1) / sizeof(T)) throw std::bad_alloc();
        if (auto p = static_cast<T*>(::operator new(n * sizeof(T)))) {
            return p;
        }
        throw std::bad_alloc();
    }

    void deallocate(T* p, std::size_t) noexcept {
        ::operator delete(p);
    }
};

template <typename T, typename U>
bool operator==(const CustomAllocator<T>&, const CustomAllocator<U>&) { return true; }

template <typename T, typename U>
bool operator!=(const CustomAllocator<T>&, const CustomAllocator<U>&) { return false; }
```

---

**Q: How does `std::function` type erasure work under the hood? Implement a custom `Function<Ret(Args...)>` with Small Buffer Optimization (SBO).**

`std::function` uses **Type Erasure** — an abstract virtual vtable interface hiding a templated concrete wrapper around any invocable target (`lambda`, function pointer, functor). Small closures fit inside an inline SBO buffer (typically 16–32 bytes) to eliminate dynamic heap allocations.

```cpp
#include <utility>
#include <new>
#include <cstddef>
#include <iostream>

template <typename Signature>
class Function;

template <typename Ret, typename... Args>
class Function<Ret(Args...)> {
    static constexpr size_t SBO_SIZE = 32;

    struct Concept {
        virtual ~Concept() = default;
        virtual Ret invoke(Args&&... args) = 0;
        virtual Concept* clone(void* storage) const = 0;
    };

    template <typename F>
    struct Model final : Concept {
        F callable;
        Model(F f) : callable(std::move(f)) {}
        Ret invoke(Args&&... args) override {
            return callable(std::forward<Args>(args)...);
        }
        Concept* clone(void* storage) const override {
            if constexpr (sizeof(Model) <= SBO_SIZE) {
                return new (storage) Model(callable);
            } else {
                return new Model(callable);
            }
        }
    };

    alignas(std::max_align_t) char sbo_storage_[SBO_SIZE];
    Concept* pimpl_ = nullptr;
    bool is_sbo_ = false;

public:
    Function() noexcept = default;

    template <typename F>
    Function(F f) {
        using ModelType = Model<F>;
        if constexpr (sizeof(ModelType) <= SBO_SIZE) {
            pimpl_ = new (sbo_storage_) ModelType(std::move(f));
            is_sbo_ = true;
        } else {
            pimpl_ = new ModelType(std::move(f));
            is_sbo_ = false;
        }
    }

    ~Function() { destroy(); }

    Function(const Function& other) {
        if (other.pimpl_) {
            pimpl_ = other.pimpl_->clone(sbo_storage_);
            is_sbo_ = (pimpl_ == reinterpret_cast<Concept*>(sbo_storage_));
        }
    }

    Ret operator()(Args... args) const {
        if (!pimpl_) throw std::bad_function_call();
        return pimpl_->invoke(std::forward<Args>(args)...);
    }

private:
    void destroy() noexcept {
        if (pimpl_) {
            if (is_sbo_) {
                pimpl_->~Concept();
            } else {
                delete pimpl_;
            }
            pimpl_ = nullptr;
        }
    }
};
```

---

**Q: What is a custom deleter? Write a `unique_ptr` that manages a `FILE*`.**

```cpp
#include <memory>
#include <cstdio>

// Using function pointer:
auto file = std::unique_ptr<FILE, decltype(&std::fclose)>(
    std::fopen("data.csv", "r"), &std::fclose
);
// sizeof(file) = 16 bytes (pointer + function pointer)

// Using stateless lambda (EBO eliminates deleter storage):
auto file2 = std::unique_ptr<FILE, decltype([](FILE* f) { std::fclose(f); })>(
    std::fopen("data.csv", "r")
);
// sizeof(file2) = 8 bytes (EBO — empty lambda has zero size as base)

// Bloomberg use case — managing database handles:
struct DBCloser {
    void operator()(DBConnection* c) const { c->disconnect(); delete c; }
};
std::unique_ptr<DBConnection, DBCloser> conn(new DBConnection("bloomberg.db"));
```

---

**Q: What is placement `new`? When would you use it?**

Placement `new` constructs an object at a pre-allocated memory address without calling `malloc`:

```cpp
alignas(alignof(Widget)) char buf[sizeof(Widget)];
Widget* w = new (buf) Widget(42);  // construct in pre-allocated buffer
// ... use w ...
w->~Widget();                       // MUST call destructor explicitly!
// Do NOT call delete — buf is stack memory, not heap
```

Use cases at Bloomberg:
1. **Arena allocators** — pre-allocate a large block, construct objects within it (zero syscalls)
2. **Object pools** — reuse memory for fixed-type objects (order book entries)
3. **Memory-mapped I/O** — construct C++ objects at specific hardware addresses

---

### Gotcha Code — Memory Management

```cpp
// Q: What's wrong with this code?
void process() {
    int* p = new int[100];
    doWork(p);   // might throw
    delete p;    // wrong delete AND leak if doWork throws
}
// A: Two bugs:
// (1) new[] must use delete[], not delete — UB (array cookies not handled).
// (2) If doWork() throws, delete is never reached → memory leak.
// Fix: use std::vector<int> or std::unique_ptr<int[]>.
```

```cpp
// Q: Is this safe?
std::shared_ptr<Widget> sp1(new Widget);
std::shared_ptr<Widget> sp2(sp1.get()); // ?
// A: NO — double delete! sp2 creates a SEPARATE control block for the
// same raw pointer. When both go out of scope, Widget is deleted twice → UB.
// Fix: copy the shared_ptr: auto sp2 = sp1;
// Or: use enable_shared_from_this if you need shared_from_this().
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

### OOP & Polymorphism

**Beginner**

**Q: Why must a base class destructor be virtual if used polymorphically?**

When deleting through a base pointer, the vtable routes to the correct (most-derived) destructor:

```cpp
class Base {
public:
    virtual ~Base() = default;  // ← MUST be virtual
};
class Derived : public Base {
    std::vector<int> data_;     // needs cleanup
};

Base* p = new Derived();
delete p;
// With virtual ~Base:    vtable → ~Derived() → ~Base() → deallocate ✓
// Without virtual ~Base: calls ~Base() directly → ~Derived() skipped → data_ leaked, UB
```

**Rule:** If a class has **any** virtual function, its destructor **must** be virtual.

---

**Intermediate**

**Q: Explain the vtable mechanism. How does dynamic dispatch work?**

```
class Base { virtual void foo(); virtual void bar(); int x; };
class Derived : public Base { void foo() override; int y; };

Derived d; layout:              Derived vtable (read-only):
┌──────────────┐                ┌─────────────────────┐
│ vptr ────────────────────────→│ &Derived::foo       │ slot 0 (overridden)
├──────────────┤                │ &Base::bar          │ slot 1 (inherited)
│ Base::x      │                │ &Derived::~Derived  │ slot 2
├──────────────┤                └─────────────────────┘
│ Derived::y   │
└──────────────┘
```

A call `base_ptr->foo()` compiles to:
```asm
mov rax, [rdi]         ; load vptr from object (*base_ptr)
call [rax + 0]         ; call vtable[0] — indirect call (~2-5ns overhead)
```

---

**Q: Compare `std::variant` + `std::visit` vs Virtual Vtable Dynamic Dispatch.**

| Metric | Virtual Vtable Polymorphism | `std::variant` + `std::visit` |
|---|---|---|
| Dispatch Mechanism | Indirect function call via `vptr` (`call [rax]`) | Switch table / jump table on type index |
| Memory Allocation | Heap pointers & polymorphic bases | Stack contiguous discriminated union |
| Cache Locality | Poor (chasing pointers across heap) | Excellent (contiguous array storage) |
| Extensibility | Open (add new derived classes without touching base) | Closed (adding types requires modifying variant) |

```cpp
#include <variant>
#include <iostream>

struct BuyOrder { double price; };
struct SellOrder { double price; };
using Order = std::variant<BuyOrder, SellOrder>;

void processOrder(const Order& order) {
    std::visit([](auto&& arg) {
        std::cout << "Price: " << arg.price << "\n";
    }, order); // Evaluated via static jump table without vptr pointers!
}
```

---

**Q: What is the vtable layout with multiple inheritance? What are adjustor thunks?**

In `class D : public A, public B`, `D` contains **two** `vptr`s. When calling an overridden method through a `B*` pointer, the vtable entry points to an **adjustor thunk** — a small instruction sequence that subtracts `sizeof(A)` from `this` before calling `D::method()`.

```
D object layout:
┌──────────┐
│ A::vptr  │ ← points to D-in-A vtable
│ A::data  │
├──────────┤
│ B::vptr  │ ← points to D-in-B vtable (with thunks)
│ B::data  │
├──────────┤
│ D::data  │
└──────────┘
```

---

**Q: What is the Rule of Three / Rule of Five / Rule of Zero?**

| Rule | When | What to declare |
|---|---|---|
| **Rule of Zero** | Class uses only RAII members (`string`, `vector`, `unique_ptr`) | Nothing — compiler generates everything correctly |
| **Rule of Three** | Class manages a raw resource (pre-C++11) | Destructor, Copy Ctor, Copy Assignment |
| **Rule of Five** | Class manages a raw resource (C++11+) | Destructor, Copy Ctor, Copy Assignment, **Move Ctor**, **Move Assignment** |

```cpp
// Rule of Zero (preferred):
class Good {
    std::string name_;
    std::vector<int> data_;
    // Compiler generates correct dtor, copy, move — nothing to declare
};

// Rule of Five (when managing raw resources):
class Raw {
    int* data_;
    size_t size_;
public:
    Raw(size_t n) : data_(new int[n]), size_(n) {}
    ~Raw() { delete[] data_; }
    Raw(const Raw& o) : data_(new int[o.size_]), size_(o.size_) {
        std::copy(o.data_, o.data_ + size_, data_);
    }
    Raw& operator=(Raw other) noexcept { swap(*this, other); return *this; }
    Raw(Raw&& o) noexcept : data_(o.data_), size_(o.size_) { o.data_ = nullptr; o.size_ = 0; }
    Raw& operator=(Raw&& o) noexcept { swap(*this, o); return *this; }
    friend void swap(Raw& a, Raw& b) noexcept {
        using std::swap; swap(a.data_, b.data_); swap(a.size_, b.size_);
    }
};
```

---

**Q: Why do destructors default to `noexcept(true)`? What happens if one throws?**

If a destructor throws during exception stack unwinding (i.e., another exception is already in flight), two active exceptions coexist → `std::terminate()` is called immediately, crashing the process. C++11 made destructors implicitly `noexcept` to prevent this.

---

### Gotcha Code — OOP

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
// The Derived object is not yet constructed, so calling Derived::init
// would access uninitialized Derived members — undefined behavior.
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
// A: Only ~Base() is called — ~Derived() never runs → memory leak (400 bytes).
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

### Templates

**Intermediate**

**Q: What is SFINAE? Give an example.**

SFINAE (Substitution Failure Is Not An Error): when the compiler substitutes template arguments and the substitution creates an invalid type, that overload is **silently removed** from the candidate set instead of causing a compile error.

```cpp
// enable_if: function only exists for integral types
template <typename T>
typename std::enable_if<std::is_integral_v<T>, T>::type
double_it(T val) { return val * 2; }

double_it(42);    // OK — int is integral
double_it(3.14);  // substitution failure → overload removed → compile error

// C++20 replacement — Concepts (much cleaner):
template <std::integral T>
T double_it(T val) { return val * 2; }
```

---

### Gotcha Code — Templates

```cpp
// Q: Does this compile? If so, what does it print?
template <typename T>
void print(T val) { std::cout << "generic: " << val << "\n"; }

template <>
void print<int>(int val) { std::cout << "int: " << val << "\n"; }

void print(int val) { std::cout << "non-template: " << val << "\n"; }

int main() {
    print(42);        // "non-template: 42" — non-template exact match wins
    print<int>(42);   // "int: 42" — explicit <int> forces template path
    print(3.14);      // "generic: 3.14" — only generic template matches double
}
```

---

### Move Semantics & Value Categories

**Beginner**

**Q: What is an lvalue? What is an rvalue?**

- **lvalue**: An expression with identity — you can take its address. Examples: `x`, `*ptr`, `arr[0]`, `obj.member`
- **rvalue**: A temporary expression — no persistent identity. Examples: `42`, `x + y`, `Widget()`, `std::move(x)`

```cpp
int x = 10;         // x is lvalue, 10 is rvalue
int& r = x;         // OK — lvalue ref binds to lvalue
// int& r2 = 42;    // ERROR — lvalue ref can't bind to rvalue
const int& r3 = 42; // OK — const lvalue ref extends temporary lifetime
int&& rr = 42;      // OK — rvalue ref binds to rvalue
```

---

**Q: What does `std::move` do?**

`std::move` does NOT move anything. It is an unconditional cast to `T&&` (rvalue reference):

```cpp
template <typename T>
constexpr std::remove_reference_t<T>&& move(T&& arg) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(arg);
}
```

---

**Intermediate**

**Q: Explain `std::move_if_noexcept` and vector reallocation guarantees. What happens if a move constructor throws?**

`std::vector` uses `std::move_if_noexcept` during memory reallocation. If an element's move constructor is NOT marked `noexcept`, `std::vector` falls back to **copying** elements. This ensures the **Strong Exception Guarantee** — if an exception is thrown mid-reallocation, the original vector elements remain un-mutated.

---

**Q: What is the state of a moved-from object?**

A moved-from object is in a **valid but unspecified** state. Destructors and assignments must work reliably.

---

**Q: What is copy elision? What is RVO vs NRVO?**

- **RVO (Return Value Optimization)**: Prvalue return constructed directly in caller's stack frame (Guaranteed in C++17).
- **NRVO (Named RVO)**: Returning a named local variable. Anti-pattern: `return std::move(local);` disables NRVO!

---

### Gotcha Code — Move Semantics

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
// A: "default" only (NRVO elides copy and move).
```

```cpp
// Q: What's wrong with this code?
std::unique_ptr<Widget> create() {
    auto w = std::make_unique<Widget>();
    return std::move(w);  // correct?
}
// A: std::move is HARMFUL here — it prevents NRVO. Just `return w;`.
```

---

### STL Containers & Iterators

**Beginner**

**Q: What is `push_back` vs `emplace_back`?**

`push_back` takes a constructed object and copies/moves it into the container. `emplace_back` forwards arguments directly to the constructor, constructing the object **in-place** inside vector memory via placement `new`.

---

**Intermediate**

**Q: Explain iterator invalidation rules for `std::vector`.**

| Operation | What's invalidated |
|---|---|
| `push_back` / `emplace_back` (no realloc) | Iterators/refs from insertion point to `end()` |
| `push_back` / `emplace_back` (realloc) | **ALL** iterators, pointers, and references |
| `erase(pos)` | Iterators/refs from erased position to `end()` |

---

**Q: What is `std::map` (Red-Black Tree) vs `std::unordered_map` (Hash Table)?**

| Property | `std::map` | `std::unordered_map` |
|---|---|---|
| Implementation | Red-black tree | Hash table with chaining |
| Order | Sorted by key | Unordered |
| `find()` | O(log N) guaranteed | O(1) average, O(N) worst |
| Memory | ~48-64 bytes per node | Bucket array + nodes |

---

**Q: What is the Small Buffer Optimization (SBO) in `std::string`?**

Short strings ($\le 15$ chars) are stored directly inside the `std::string` object buffer without dynamic heap allocation.

---

### Concurrency & Multithreading

**Beginner**

**Q: What is a mutex? Why do you need it?**

A mutex prevents multiple threads from accessing shared data simultaneously, eliminating **data races** (UB).

---

**Intermediate**

**Q: What is `std::lock_guard` vs `std::unique_lock`?**

`lock_guard` is a minimal RAII lock wrapper. `unique_lock` allows deferred locking, manual `unlock()`, and integration with `std::condition_variable`.

---

**Q: What are condition variables? What are spurious wakeups?**

Condition variables allow threads to sleep until notified. Spurious wakeups occur when threads wake without notification — ALWAYS wrap `cv.wait()` inside a predicate loop: `cv.wait(lock, [&]{ return ready; });`.

---

**Q: What is `std::scoped_lock` (C++17)?**

`std::scoped_lock` acquires multiple mutexes atomically using a deadlock-avoidance algorithm (try-and-back-off).

---

**Q: Explain `std::atomic_ref` and `std::atomic<std::shared_ptr<T>>` (C++20).**

- `std::atomic_ref<T>` (C++20): Allows non-atomic objects to be accessed atomically without changing their struct layout.
- `std::atomic<std::shared_ptr<T>>` (C++20): Lock-free atomic shared pointer updates without manual `std::atomic_load`/`store` functions.

---

**Q: Explain the exact differences between `volatile` and `std::atomic` in hardware MMIO vs multithreading.**

- `volatile`: Tells compiler memory can change externally (Hardware MMIO). Prevents read/write elision optimization. **Zero multithreaded synchronization!**
- `std::atomic`: Provides atomic operations, CPU bus locks, and memory fences across CPU cores.

---

### Gotcha Code — Concurrency

```cpp
// Q: Is this thread-safe?
bool ready = false;

void producer() { ready = true; }
void consumer() { while (!ready) {} }
// A: NO — data race on `ready`. Compiler may optimize loop to infinite spin.
// Fix: std::atomic<bool> ready{false};
```

---

### Const Correctness & Type Casting

**Intermediate**

**Q: Explain assembly costs of `static_cast`, `dynamic_cast`, `const_cast`, and `reinterpret_cast`. What is the Strict Aliasing Rule (`tbaa`)?**

- `static_cast`: Zero runtime cost (compile-time type adjustment, pointer offset addition for base/derived).
- `const_cast`: Zero runtime cost (toggles const qualifier).
- `reinterpret_cast`: Zero runtime cost (reinterprets bit patterns).
- `dynamic_cast`: High runtime cost (~50-200ns) — performs `typeinfo` string/tree traversal.
- **Strict Aliasing Rule (`tbaa`)**: Compilers assume pointers of different types (e.g. `int*` and `float*`) do not alias the same memory address, enabling aggressive register caching.

---

**Q: Explain Itanium ABI `.eh_frame` DWARF exception unwinding. Why are exceptions zero-cost on the happy path?**

Modern C++ compilers use **Zero-Cost Table-Driven Exception Handling**. No try/catch code instructions execute on the happy path ($0$ CPU overhead). When an exception is thrown (`throw`), the runtime parses `.eh_frame` DWARF tables to unwind stack frames and locate catch blocks.

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
| `std::move` on return | Prevents NRVO copy elision | Just `return local;` — compiler applies implicit move |
| `string_view` to C API | Non-null-terminated `.data()` reads past boundary | Construct `std::string(sv)` first |
| `<=` in sort comparator | Violates strict weak ordering → heap corruption UB | Use `<` (strict less-than) |
