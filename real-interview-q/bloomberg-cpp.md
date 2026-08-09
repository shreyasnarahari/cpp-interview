# Bloomberg — C++ Language & Mechanics Questions

> **Focus:** RAII, const correctness, OOP, thread safety, memory management, STL internals
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
// Evaluation order of function arguments is unspecified.

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

In hot paths (Bloomberg ticker processing), `shared_ptr` copies can become a measurable bottleneck due to atomic refcount operations on every copy/destruction.

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

**Q: Explain `enable_shared_from_this` — how does it prevent double deletion?**

`enable_shared_from_this<T>` adds an internal private `weak_ptr<T>` to the class. When the object is first wrapped in a `shared_ptr`, the constructor detects this base and initializes the internal `weak_ptr`. Calling `shared_from_this()` then promotes that `weak_ptr` to a `shared_ptr`, **reusing the existing control block** instead of creating a duplicate.

```cpp
class Widget : public std::enable_shared_from_this<Widget> {
public:
    std::shared_ptr<Widget> get_self() {
        return shared_from_this();  // safe — reuses existing control block
        // return std::shared_ptr<Widget>(this);  // BUG — double delete!
    }
};

auto sp1 = std::make_shared<Widget>();
auto sp2 = sp1->get_self();  // sp1 and sp2 share the SAME control block
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
mov rax, [rdi]         ; load vptr from object
call [rax + 0]         ; call vtable[0] — indirect call (~5ns overhead)
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
                  // (no other overload exists)

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
// Overload resolution priority:
// 1. Non-template exact match (highest)
// 2. Template specialization
// 3. Template (deduced)
// 4. Non-template with conversion (lowest)
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
// Simplified implementation:
template <typename T>
constexpr std::remove_reference_t<T>&& move(T&& arg) noexcept {
    return static_cast<std::remove_reference_t<T>&&>(arg);
}

std::string s = "bloomberg";
std::string s2 = std::move(s);  
// Step 1: std::move(s) → static_cast<string&&>(s) — just a cast, 0 cost
// Step 2: string's MOVE constructor is selected (receives string&&)
// Step 3: Move ctor steals s's internal buffer (pointer swap)
// Step 4: s is now in a "valid but unspecified" state (typically empty)
```

**Critical trap:** A named rvalue reference is itself an lvalue:
```cpp
void process(std::string&& s) {
    std::string local = s;            // COPIES! s has a name → it's an lvalue
    std::string local2 = std::move(s); // MOVES — std::move re-casts to rvalue
}
```

---

**Intermediate**

**Q: What is the state of a moved-from object?**

A moved-from object is in a **valid but unspecified** state. The only operations guaranteed to work:
- Destruction (`~T()`)
- Assignment (`operator=`)
- Any operation with no preconditions

```cpp
std::string s = "data";
std::string s2 = std::move(s);
// s is valid but unspecified — likely empty, but NOT guaranteed
// s.size(); // legal but unspecified result
// s = "new"; // legal — reassignment always works
```

---

**Q: Why should move operations be `noexcept`?**

`std::vector` uses `move_if_noexcept` during reallocation. If a move constructor might throw, vector **falls back to copying** to maintain the strong exception guarantee:

```cpp
class Widget {
public:
    Widget(Widget&&) noexcept;  // ← vector will MOVE during realloc (fast)
    // Widget(Widget&&);        // without noexcept → vector will COPY (slow!)
};

// Performance impact at Bloomberg:
// A vector of 10,000 market data entries reallocating:
//   With noexcept: 10,000 pointer swaps (~100 µs)
//   Without:       10,000 deep copies (~10 ms) — 100x slower!
```

---

**Q: What is copy elision? What is RVO vs NRVO?**

**RVO (Return Value Optimization):** Returning a prvalue — guaranteed since C++17:
```cpp
Widget create() { return Widget(42); }  // Widget constructed directly in caller's space
Widget w = create();  // only ONE constructor call — no copy, no move
```

**NRVO (Named RVO):** Returning a named local — allowed but NOT guaranteed:
```cpp
Widget create() {
    Widget w(42);
    return w;          // compiler MAY construct w directly in caller's space
}
// Anti-pattern: return std::move(w); — PREVENTS NRVO!
```

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
// A: Most likely "default" only (NRVO elides the move/copy).
// Without NRVO: "default" then "move". Copy is never used here.
```

```cpp
// Q: What's wrong with this code?
std::unique_ptr<Widget> create() {
    auto w = std::make_unique<Widget>();
    return std::move(w);  // correct?
}
// A: It works, but std::move is HARMFUL here — it prevents NRVO.
// The compiler applies implicit move for local variables returned by value.
// Fix: just `return w;`
```

---

### STL Containers & Iterators

**Beginner**

**Q: What is `push_back` vs `emplace_back`?**

`push_back` takes a constructed object and copies/moves it into the container. `emplace_back` forwards arguments directly to the constructor, constructing the object **in-place** inside the container's memory:

```cpp
std::vector<std::pair<std::string, int>> v;

v.push_back({"bloomberg", 42});   // constructs temporary pair, then moves it
v.emplace_back("bloomberg", 42);  // constructs pair directly inside vector memory
// emplace_back uses placement new — avoids the temporary entirely
```

---

**Intermediate**

**Q: Explain iterator invalidation rules for `std::vector`.**

| Operation | What's invalidated |
|---|---|
| `push_back` / `emplace_back` (no realloc) | Iterators/refs from insertion point to `end()` |
| `push_back` / `emplace_back` (realloc) | **ALL** iterators, pointers, and references |
| `insert(pos, val)` | Same rules as push_back |
| `erase(pos)` | Iterators/refs from erased position to `end()` |
| `clear()` | All iterators/refs |
| `reserve()` | All (if reallocation occurs) |

**Full container matrix:**

| Container | Insert invalidates | Erase invalidates |
|---|---|---|
| `vector` | All if realloc; else at/after point | At/after erased element |
| `deque` | All iterators (refs valid if front/back) | All iterators (refs valid if front/back) |
| `list` / `forward_list` | Nothing | Only erased element |
| `map` / `set` | Nothing | Only erased element |
| `unordered_map` / `unordered_set` | All if rehash; else nothing | Only erased element |

---

**Q: What is `std::map` (Red-Black Tree) vs `std::unordered_map` (Hash Table)?**

| Property | `std::map` | `std::unordered_map` |
|---|---|---|
| Implementation | Red-black tree | Hash table with chaining |
| Order | Sorted by key | Unordered |
| `find()` | O(log N) guaranteed | O(1) average, O(N) worst |
| `insert()` | O(log N) | O(1) amortized |
| Iterator stability | Insert never invalidates | Rehash invalidates all |
| Memory | ~48-64 bytes per node (separate allocs) | Bucket array + nodes |
| When faster | Small N, need ordering, range queries | Large N, good hash |

---

**Q: What is the Small Buffer Optimization (SBO) in `std::string`?**

Short strings are stored **directly inside** the `std::string` object (no heap allocation):

```
Short string (≤ 15 chars on most implementations):
┌────────────────────────────────────────┐
│ "bloomberg\0" stored inline (no heap!) │  sizeof(string) ≈ 32 bytes
└────────────────────────────────────────┘

Long string (> 15 chars):
┌────────────────────┐
│ ptr ───→ heap buf  │  heap allocation for the actual character data
│ size               │
│ capacity           │
└────────────────────┘
```

SBO eliminates heap allocations for the vast majority of strings in practice. At Bloomberg, ticker symbols ("AAPL", "MSFT") always fit in SBO — zero allocation overhead.

---

**Q: Custom hash function for user-defined structs**

```cpp
struct OrderKey { 
    int id; 
    std::string symbol; 
    
    bool operator==(const OrderKey&) const = default;  // C++20
};

struct OrderKeyHash {
    size_t operator()(const OrderKey& k) const {
        size_t h1 = std::hash<int>{}(k.id);
        size_t h2 = std::hash<std::string>{}(k.symbol);
        return h1 ^ (h2 << 1);  // combine hashes
    }
};

std::unordered_map<OrderKey, double, OrderKeyHash> order_prices;
```

---

**Q: What is `std::span` (C++20)?**

A lightweight, non-owning view over contiguous memory — like `string_view` for arrays:

```cpp
void process_bytes(std::span<const uint8_t> buffer) {
    // Accepts std::vector, std::array, or C-array — zero copies!
    for (auto byte : buffer) { /* ... */ }
}

std::vector<uint8_t> vec = {0x01, 0x02, 0x03};
uint8_t arr[] = {0x04, 0x05};
process_bytes(vec);   // OK
process_bytes(arr);   // OK
process_bytes({vec.data() + 1, 2});  // subrange view
```

---

### Gotcha Code — STL

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
// Fix: it = v.erase(it); and skip ++it when erasing:
for (auto it = v.begin(); it != v.end(); ) {
    if (*it % 2 == 0) it = v.erase(it);
    else ++it;
}
// Or C++20: std::erase_if(v, [](int x) { return x % 2 == 0; });
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
// Fix: use index-based access, or reserve() beforehand.
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

**Remove-Erase idiom:**
```cpp
// Erase all odd elements efficiently:
v.erase(std::remove_if(v.begin(), v.end(), [](int x) { return x % 2 != 0; }), v.end());

// C++20 simplification:
std::erase_if(v, [](int x) { return x % 2 != 0; });
```

**Strict Weak Ordering trap:**
```cpp
// Using <= in std::sort violates irreflexivity → HEAP CORRUPTION UB!
std::sort(v.begin(), v.end(), [](int a, int b) { return a <= b; });  // BUG!
// comp(x, x) MUST return false. <= returns true for equal elements.
// Fix: use < (strict less-than)
```

---

### Concurrency & Multithreading

**Beginner**

**Q: What is a mutex? Why do you need it?**

A mutex (mutual exclusion) prevents multiple threads from accessing shared data simultaneously. Without it, concurrent reads and writes to the same memory location cause **data races** (undefined behavior).

```cpp
std::mutex mtx;
int shared_counter = 0;

void safe_increment() {
    std::lock_guard<std::mutex> lock(mtx);  // RAII lock
    shared_counter++;                        // protected access
}   // lock released automatically (even if an exception is thrown)
```

---

**Intermediate**

**Q: What is `std::lock_guard` vs `std::unique_lock`?**

| Feature | `lock_guard` | `unique_lock` |
|---|---|---|
| Lock timing | Locks on construction only | Can lock/unlock at any time |
| Manual unlock | Not possible | `unlock()` available |
| Condition variables | Cannot be used with `cv.wait()` | Required for `cv.wait()` |
| Deferred locking | Not supported | `std::defer_lock` option |
| Moveable | No | Yes |
| Overhead | Slightly lower | Slightly higher (tracks lock state) |

```cpp
// lock_guard — simple, always-locked RAII wrapper:
{
    std::lock_guard<std::mutex> lock(mtx);
    // ... critical section ...
}   // auto-unlock

// unique_lock — flexible, needed for condition variables:
{
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&]{ return ready; });  // releases lock while waiting
    // ... process data ...
}   // auto-unlock
```

---

**Q: What are condition variables? What are spurious wakeups?**

A condition variable allows a thread to sleep until notified. **Spurious wakeups** are OS-level events where a thread wakes without notification — this is why you MUST use a predicate:

```cpp
std::mutex mtx;
std::condition_variable cv;
bool data_ready = false;

// WRONG — vulnerable to spurious wakeups:
cv.wait(lock);  // may wake randomly!

// CORRECT — predicate loop:
cv.wait(lock, [&]{ return data_ready; });
// Equivalent to: while (!data_ready) cv.wait(lock);
```

---

**Q: What is `std::scoped_lock` (C++17)?**

`scoped_lock` locks **multiple mutexes simultaneously** using a deadlock-avoidance algorithm (try-and-back-off):

```cpp
std::mutex m1, m2;

// DEADLOCK-PRONE:
void bad_transfer() {
    std::lock_guard l1(m1);  // Thread A: locks m1, then waits for m2
    std::lock_guard l2(m2);  // Thread B: locks m2, then waits for m1 → DEADLOCK
}

// SAFE:
void safe_transfer() {
    std::scoped_lock lock(m1, m2);  // Locks both atomically, no deadlock possible
}
```

---

**Q: What is `std::atomic`? When to use it vs a mutex?**

| Use case | `std::atomic` | `std::mutex` |
|---|---|---|
| Single variable (counter, flag) | ✅ Preferred | Overkill |
| Multiple variables in one operation | ❌ Can't protect multiple | ✅ Critical section |
| Latency | ~5-25 ns | ~25-50 ns (uncontended) |
| Contended access | Busy-wait (CPU burn) | Kernel sleep (efficient) |

```cpp
// Atomic — perfect for single counters:
std::atomic<int> counter{0};
counter.fetch_add(1, std::memory_order_relaxed);  // lock-free, ~5ns

// Mutex — required for compound operations:
std::mutex mtx;
void transfer(Account& a, Account& b, int amount) {
    std::lock_guard lock(mtx);
    a.balance -= amount;  // both must happen
    b.balance += amount;  // atomically together
}
```

---

**Advanced**

**Q: Implement a thread-safe singleton.**

```cpp
class Singleton {
public:
    static Singleton& instance() {
        static Singleton s;  // Thread-safe since C++11 (§6.7/4)
        return s;            // Compiler uses double-checked locking internally
    }
    
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    
private:
    Singleton() = default;
};
```

---

**Q: `std::atomic` memory orderings**

```
relaxed:      Atomicity only, no ordering guarantees. Use for counters.
acquire:      Reads after this see writes from the releasing thread.
release:      Writes before this are visible to acquiring threads.
acq_rel:      Both acquire and release (for read-modify-write ops).
seq_cst:      Total global ordering — default, most expensive (~MFENCE on x86).
```

---

**Q: What is false sharing? How to fix it?**

When two threads modify variables sharing the same 64-byte cache line, each write invalidates the other core's cache → constant cross-core traffic:

```cpp
// BAD — false sharing:
struct Counters {
    std::atomic<uint64_t> a;  // thread 1
    std::atomic<uint64_t> b;  // thread 2 — same cache line!
};

// GOOD — cache line isolation:
struct alignas(64) ThreadCounter {
    std::atomic<uint64_t> count{0};
};
ThreadCounter counters[2];  // each on its own 64-byte cache line
```

---

### Gotcha Code — Concurrency

```cpp
// Q: What's wrong with this code?
std::mutex mtx;
int counter = 0;

void increment() {
    mtx.lock();
    counter++;
    if (counter > 100) {
        return;  // BUG! Mutex never unlocked!
    }
    mtx.unlock();
}
// A: If counter > 100, function returns without unlocking → deadlock on next call.
// Fix: use std::lock_guard<std::mutex> lock(mtx); — RAII guarantees unlock.
```

```cpp
// Q: Is this thread-safe?
bool ready = false;

void producer() {
    ready = true;  // signal consumer
}

void consumer() {
    while (!ready) {}  // spin-wait
}
// A: NO — data race on `ready`. Non-atomic bool accessed concurrently.
// Compiler may optimize away the loop (ready never changes from consumer's perspective).
// Fix: std::atomic<bool> ready{false};
```

```cpp
// Q: Does this avoid deadlock?
std::mutex m1, m2;

void threadA() {
    std::lock_guard l1(m1);
    std::lock_guard l2(m2);   // locks m1 then m2
}
void threadB() {
    std::lock_guard l1(m2);
    std::lock_guard l2(m1);   // locks m2 then m1 — OPPOSITE ORDER!
}
// A: NO — classic deadlock. threadA holds m1, waits for m2.
// threadB holds m2, waits for m1. Circular wait.
// Fix: std::scoped_lock lock(m1, m2); — deadlock avoidance algorithm.
```

---

### Lambda Expressions

**Intermediate**

**Q: Lambda capture modes — explain `[=]`, `[&]`, `[x]`, `[&x]`, `[this]`, `[*this]`.**

| Capture | Meaning | Danger |
|---|---|---|
| `[=]` | All locals by value (copy) | May copy large objects unexpectedly |
| `[&]` | All locals by reference | Dangling if lambda outlives scope |
| `[x]` | Specific variable by value | Safe copy |
| `[&x]` | Specific variable by reference | Dangling risk |
| `[this]` | Captures `this` pointer | Dangling if object is destroyed |
| `[*this]` | Captures copy of `*this` (C++17) | Safe — independent copy |

---

**Q: What is the difference between a lambda and `std::function`?**

| Property | Lambda | `std::function` |
|---|---|---|
| Type | Unique unnamed class | `std::function<R(Args...)>` |
| Size | Only captured state | ~32-48 bytes (SBO + vtable) |
| Overhead | Zero — inlineable | ~2-5 ns indirect call |
| Assignable | Cannot reassign to different lambda | Can hold any matching callable |
| Use case | Templates, algorithms, hot paths | Type erasure, storing heterogeneous callables |

---

### Gotcha Code — Lambdas

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
// A: "15 20 10" — lambda captures x BY VALUE. mutable lets it modify
// its own internal copy. The original x is never changed.
// State persists between calls (the lambda object owns its copy).
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
// Fix: capture by value [message] or [msg = std::move(message)]
```

---

### Const Correctness & Type Casting

**Beginner**

**Q: What is a const member function?**

A `const` member function promises not to modify the object's state. Inside it, `this` becomes `const T*`:

```cpp
class Widget {
    int data_;
public:
    int get() const {   // this = const Widget*
        // data_++;     // ERROR — can't modify members
        return data_;
    }
    void set(int v) {   // this = Widget*
        data_ = v;      // OK — non-const
    }
};

const Widget cw;
cw.get();    // OK — const method on const object
// cw.set(5); // ERROR — can't call non-const method on const object
```

---

**Intermediate**

**Q: What is `mutable`? Give a real use case.**

`mutable` allows modification in `const` methods — for **logical constness** (object appears unchanged externally):

```cpp
class PriceCache {
    mutable std::mutex mtx_;              // must lock in const methods
    mutable std::optional<double> cache_; // cache computed results
public:
    double getPrice(const std::string& symbol) const {
        std::lock_guard lock(mtx_);       // OK — mutable
        if (cache_) return *cache_;
        double price = expensive_lookup(symbol);
        cache_ = price;                   // OK — mutable
        return price;
    }
};
```

---

**Q: What is `constexpr` vs `const`?**

| | `const` | `constexpr` |
|---|---|---|
| Meaning | Value cannot be modified | Value **may** be computed at compile time |
| Runtime init | Yes | Only if expression is compile-time evaluable |
| Functions | N/A | Function can be evaluated at compile time or runtime |

```cpp
const int a = runtime_func();     // OK — initialized at runtime
constexpr int b = 42;             // OK — compile-time constant
// constexpr int c = runtime_func(); // ERROR — not a constant expression
```

---

**Q: Top-level const vs low-level const**

```cpp
const int* p;       // Low-level: pointed-to data is const (can't modify *p)
int* const p;       // Top-level: pointer itself is const (can't reassign p)
const int* const p; // Both: can't modify *p or reassign p
```

---

**Q: `consteval` (C++20) vs `constexpr`**

```cpp
consteval double calc_margin(double price) {
    return price * 0.05 + 0.10;
}
constexpr double margin = calc_margin(100.0);  // 5.10 — computed at compile time!
// double m2 = calc_margin(runtime_val);        // ERROR — consteval requires compile-time args
```

---

### Gotcha Code — Const Correctness

```cpp
// Q: Does this compile?
const int ci = 42;
int* p = const_cast<int*>(&ci);
*p = 100;
std::cout << ci << " " << *p;
// A: Compiles, but *p = 100 is UNDEFINED BEHAVIOR because ci is
// truly const (may be in .rodata). Compiler may have inlined 42.
// const_cast is only safe when the original was actually non-const.
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
```

---

### Exception Safety & RAII Patterns

**Q: Exception safety guarantees & the copy-and-swap idiom**

```cpp
class SafeBuffer {
    size_t size_;
    int* data_;
public:
    SafeBuffer(size_t s) : size_(s), data_(new int[s]) {}
    ~SafeBuffer() { delete[] data_; }
    
    // Copy Constructor
    SafeBuffer(const SafeBuffer& other) 
        : size_(other.size_), data_(new int[other.size_]) {
        std::copy(other.data_, other.data_ + size_, data_);
    }

    // Copy-and-Swap gives STRONG exception safety:
    // Step 1: Copy into `other` (if copy throws, *this is untouched)
    // Step 2: Swap is noexcept — always succeeds
    // Step 3: Old data destroyed when `other` goes out of scope
    SafeBuffer& operator=(SafeBuffer other) noexcept {  // pass by VALUE
        swap(*this, other);
        return *this;
    }

    friend void swap(SafeBuffer& a, SafeBuffer& b) noexcept {
        using std::swap;
        swap(a.size_, b.size_);
        swap(a.data_, b.data_);
    }
};
```

---

### Pimpl Idiom

**Q: What is the Pimpl idiom? Implement it.**

```cpp
// Header: MarketFeed.h — stable ABI, fast compilation
#include <memory>
class MarketFeed {
    struct Impl;                   // forward declaration — incomplete type
    std::unique_ptr<Impl> impl_;   // works with incomplete types
public:
    MarketFeed();
    ~MarketFeed();                 // MUST be declared here, defined in .cpp
    MarketFeed(MarketFeed&&) noexcept;
    MarketFeed& operator=(MarketFeed&&) noexcept;
    void connect();
    double getPrice(const std::string& symbol) const;
};

// Source: MarketFeed.cpp — can change freely without recompiling consumers
struct MarketFeed::Impl {
    DBConnection db;               // heavy header only included here
    NetworkSocket socket;
    std::unordered_map<std::string, double> cache;
};

MarketFeed::MarketFeed() : impl_(std::make_unique<Impl>()) {}
MarketFeed::~MarketFeed() = default;  // defined WHERE Impl is complete
MarketFeed::MarketFeed(MarketFeed&&) noexcept = default;
MarketFeed& MarketFeed::operator=(MarketFeed&&) noexcept = default;

void MarketFeed::connect() { impl_->socket.connect(); }
```

---

### Advanced Topics Quick Reference

**`std::string_view` trap with C APIs:**
- Passing `.data()` from a non-null-terminated `string_view` into C APIs (`fopen`, `printf`) reads past the view boundary → UB.
- Fix: construct `std::string(view)` before passing to C APIs.

**`volatile` vs `std::atomic`:**
- `volatile`: For memory-mapped I/O hardware registers (prevents compiler from optimizing away reads/writes).
- `std::atomic`: For multithreaded synchronization (provides memory ordering and visibility guarantees).

**`std::expected` (C++23) vs exceptions:**
- Non-allocating tagged union for error propagation without stack unwinding overhead.
- Monadic operations: `.and_then()`, `.or_else()`, `.transform()`.

**ASan debugging:**
- Compile: `g++ -fsanitize=address -g -O1 program.cpp`
- Core dump analysis: `gdb ./program core` → `bt` → `thread apply all bt`

**Modern CMake target visibility:**
- `PRIVATE`: target's own build needs only
- `PUBLIC`: target + all downstream consumers
- `INTERFACE`: downstream consumers only (header-only libs)

**Struct alignment optimization:**
- Order fields largest-to-smallest alignment to minimize padding (up to 50% size reduction)

**C++20 Ranges lazy evaluation:**
```cpp
auto view = nums | std::views::filter([](int x) { return x % 2 == 0; });
// Zero intermediate allocations — lazy pull-based pipeline
```

**C++23 deducing this:**
```cpp
struct Data {
    template <typename Self>
    auto&& get(this Self&& self) { return std::forward<Self>(self).value_; }
    int value_;
};
```

---

### Thread-Safe Data Structures

**Thread-safe bounded queue (producer-consumer):**
```cpp
template <typename T>
class BoundedQueue {
    std::queue<T> q_;
    size_t capacity_;
    std::mutex mtx_;
    std::condition_variable cv_push_, cv_pop_;
public:
    explicit BoundedQueue(size_t cap) : capacity_(cap) {}

    void push(T val) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_push_.wait(lock, [&]{ return q_.size() < capacity_; });
        q_.push(std::move(val));
        cv_pop_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_pop_.wait(lock, [&]{ return !q_.empty(); });
        T val = std::move(q_.front());
        q_.pop();
        cv_push_.notify_one();
        return val;
    }
};
```

**Reader-writer lock with `std::shared_mutex` (C++17):**
```cpp
class SharedMarketData {
    std::unordered_map<std::string, double> prices_;
    mutable std::shared_mutex rw_mtx_;
public:
    double getPrice(const std::string& symbol) const {
        std::shared_lock lock(rw_mtx_);   // shared read — multiple readers OK
        return prices_.at(symbol);
    }
    void updatePrice(const std::string& symbol, double price) {
        std::unique_lock lock(rw_mtx_);   // exclusive write — blocks all readers
        prices_[symbol] = price;
    }
};
```

**Thread-safe event bus with `std::weak_ptr`:**
```cpp
class EventObserver { 
public: 
    virtual ~EventObserver() = default; 
    virtual void onEvent() = 0; 
};

class EventBus {
    std::vector<std::weak_ptr<EventObserver>> observers_;
    std::mutex mtx_;
public:
    void subscribe(std::shared_ptr<EventObserver> obs) {
        std::lock_guard lock(mtx_);
        observers_.push_back(obs);
    }

    void publish() {
        std::lock_guard lock(mtx_);
        for (auto it = observers_.begin(); it != observers_.end(); ) {
            if (auto sp = it->lock()) {
                sp->onEvent();
                ++it;
            } else {
                it = observers_.erase(it);  // auto-prune dead observers
            }
        }
    }
};
```

**Thread pool:**
```cpp
class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;
public:
    explicit ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(mtx_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }
    ~ThreadPool() {
        { std::unique_lock lock(mtx_); stop_ = true; }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }
};
```

**`std::jthread` with cooperative cancellation (C++20):**
```cpp
void worker(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        // Do work until cancellation
    }
}
int main() {
    std::jthread t(worker);  // Auto-joins on destructor!
}
```

---

### Lock-Free SPSC Ring Buffer

```cpp
template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    alignas(64) std::atomic<size_t> head_{0};  // consumer (cache line 1)
    alignas(64) std::atomic<size_t> tail_{0};  // producer (cache line 2)
    alignas(64) std::array<T, Capacity> buffer_;

public:
    bool push(const T& item) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        if (t - h >= Capacity) return false;          // full
        buffer_[t & (Capacity - 1)] = item;
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t t = tail_.load(std::memory_order_acquire);
        if (h == t) return std::nullopt;              // empty
        T item = buffer_[h & (Capacity - 1)];
        head_.store(h + 1, std::memory_order_release);
        return item;
    }
};
```

---

### Custom Arena Allocator

```cpp
template <size_t Size>
class FixedArena {
    alignas(std::max_align_t) char buffer_[Size];
    size_t offset_ = 0;
public:
    void* allocate(size_t n, size_t alignment = alignof(std::max_align_t)) {
        size_t addr = reinterpret_cast<size_t>(buffer_ + offset_);
        size_t aligned = (addr + alignment - 1) & ~(alignment - 1);
        size_t next = (aligned - reinterpret_cast<size_t>(buffer_)) + n;
        if (next > Size) throw std::bad_alloc();
        offset_ = next;
        return reinterpret_cast<void*>(aligned);
    }
    void reset() noexcept { offset_ = 0; }
};
```

### Advanced Language Mechanics & Deep-Dive Implementations

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

**Q: How do custom allocators work in C++? Implement a C++11 compliant `allocator<T>` template.**

Custom allocators decouple memory allocation from object construction. A standard allocator must provide `value_type`, `allocate(n)`, and `deallocate(p, n)`:

```cpp
#include <cstddef>
#include <new>
#include <utility>

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

**Q: Explain `std::move_if_noexcept` and vector reallocation guarantees. What happens if a move constructor throws?**

`std::vector` uses `std::move_if_noexcept` during memory reallocation. If an element's move constructor is NOT marked `noexcept`, `std::vector` falls back to **copying** elements. This ensures the **Strong Exception Guarantee** — if an exception is thrown mid-reallocation, the original vector elements remain un-mutated.

---

**Q: Explain `std::atomic_ref` and `std::atomic<std::shared_ptr<T>>` (C++20). How do lock-free atomic shared pointers work?**

- `std::atomic_ref<T>` (C++20): Allows non-atomic objects to be accessed atomically without changing their struct layout.
- `std::atomic<std::shared_ptr<T>>` (C++20): Lock-free atomic shared pointer updates without manual `std::atomic_load`/`store` functions.

---

**Q: Explain the exact differences between `volatile` and `std::atomic` in hardware MMIO vs multithreading.**

- `volatile`: Tells compiler memory can change externally (Hardware MMIO). Prevents read/write elision optimization. **Zero multithreaded synchronization!**
- `std::atomic`: Provides atomic operations, CPU bus locks, and memory fences across CPU cores.

---

**Q: Explain assembly costs of `static_cast`, `dynamic_cast`, `const_cast`, and `reinterpret_cast`. What is the Strict Aliasing Rule (`tbaa`)?**

- `static_cast`: Zero runtime cost (compile-time type adjustment, pointer offset addition for base/derived).
- `const_cast`: Zero runtime cost (toggles const qualifier).
- `reinterpret_cast`: Zero runtime cost (reinterprets bit patterns).
- `dynamic_cast`: High runtime cost (~50-200ns) — performs `typeinfo` string/tree traversal.
- **Strict Aliasing Rule (`tbaa`)**: Compilers assume pointers of different types (e.g. `int*` and `float*`) do not alias the same memory address, enabling aggressive register caching.

---

**Q: Explain Itanium ABI `.eh_frame` DWARF exception unwinding. Why are zero-cost exceptions $0$-cost on the happy path but expensive when thrown?**

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
