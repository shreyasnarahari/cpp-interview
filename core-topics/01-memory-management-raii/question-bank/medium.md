# Memory Management & RAII — Medium Questions

---

**Q1. What is the difference between `std::unique_ptr` and `std::shared_ptr`?**

A:

| | `unique_ptr` | `shared_ptr` |
|---|---|---|
| Ownership | Exclusive — one owner | Shared — reference counted |
| Copyable | No (move-only) | Yes (increments refcount) |
| Overhead | Zero (same as raw pointer) | Control block + atomic refcount ops |
| Size | 1 pointer (or 2 with custom deleter) | 2 pointers (object + control block) |
| Thread safety | Not thread-safe | Refcount is atomic; object access is NOT |
| Use case | Default choice for ownership | Only when truly shared |

```cpp
auto u = std::make_unique<Widget>();
// auto u2 = u;  // ERROR — unique, can't copy
auto u2 = std::move(u);  // OK — transfers ownership

auto s = std::make_shared<Widget>();
auto s2 = s;  // OK — refcount = 2
// s and s2 both keep Widget alive
```

---

**Q2. Why prefer `std::make_shared` over `std::shared_ptr<T>(new T)`?**

A: Two reasons:

1. **Single allocation**: `make_shared` allocates the object AND control block in one allocation. `shared_ptr<T>(new T)` does two allocations.

2. **Exception safety** (pre-C++17):
```cpp
// Potentially unsafe before C++17:
process(std::shared_ptr<Widget>(new Widget), computePriority());
// If computePriority() throws AFTER new Widget but BEFORE
// shared_ptr is constructed → memory leak.
// C++17 fixed evaluation order, but make_shared is still preferred.
```

Downside of `make_shared`: the memory for the object is not freed until ALL `weak_ptr`s are gone (because the control block is in the same allocation).

---

**Q3. When would you use `std::weak_ptr`?**

A: Two main use cases:

1. **Break circular references:**
```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;  // weak — doesn't prevent destruction
};
```

2. **Non-owning observer** — check if an object still exists:
```cpp
std::weak_ptr<Widget> observer = someSharedPtr;
// Later:
if (auto sp = observer.lock()) {
    sp->doSomething();  // object still alive
} else {
    // object was destroyed
}
```

---

**Q4. What is `enable_shared_from_this`?**

A: A base class that allows an object to create a `shared_ptr` to itself from within a member function.

```cpp
class Widget : public std::enable_shared_from_this<Widget> {
public:
    std::shared_ptr<Widget> getPtr() {
        return shared_from_this();  // safe — same control block
    }
};

// Usage:
auto w = std::make_shared<Widget>();
auto w2 = w->getPtr();  // w2 shares ownership with w
```

**Why is this needed?** Without it, you'd be tempted to do `shared_ptr<Widget>(this)`, which creates a SECOND control block for the same raw pointer → double delete → UB.

---

**Q5. Is `std::shared_ptr` thread-safe?**

A: **Partially:**
- The **reference count** is atomic — safe to copy/destroy shared_ptrs from different threads
- The **managed object** is NOT synchronized — you still need a mutex to access the object concurrently

```cpp
auto sp = std::make_shared<int>(42);

// Thread 1: auto sp2 = sp;   // OK — atomic refcount increment
// Thread 2: auto sp3 = sp;   // OK — same

// Thread 1: *sp = 100;       // NOT OK without synchronization
// Thread 2: *sp = 200;       // Data race!
```

---

## Coding Questions

---

**C1. Simplified `UniquePtr<T>`**

Implement a basic unique pointer with constructor, destructor, move operations, `get()`, `release()`, and `reset()`.

A:
```cpp
template <typename T>
class UniquePtr {
    T* ptr_ = nullptr;
public:
    explicit UniquePtr(T* p = nullptr) : ptr_(p) {}

    ~UniquePtr() { delete ptr_; }

    // Non-copyable
    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    // Move constructor
    UniquePtr(UniquePtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    // Move assignment
    UniquePtr& operator=(UniquePtr&& other) noexcept {
        if (this != &other) {
            delete ptr_;
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* get() const { return ptr_; }

    T* release() {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }

    void reset(T* p = nullptr) {
        delete ptr_;
        ptr_ = p;
    }

    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }

    explicit operator bool() const { return ptr_ != nullptr; }
};
```

Key details: explicit constructor (prevent implicit conversion), noexcept on moves, self-assignment check.

---

**C2. RAII Mutex Lock**

Implement a `ScopedLock` class that locks a mutex in constructor and unlocks in destructor (like `std::lock_guard`).

A:
```cpp
class ScopedLock {
    std::mutex& mtx_;
public:
    explicit ScopedLock(std::mutex& m) : mtx_(m) {
        mtx_.lock();
    }

    ~ScopedLock() {
        mtx_.unlock();
    }

    // Non-copyable, non-movable
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
    ScopedLock(ScopedLock&&) = delete;
    ScopedLock& operator=(ScopedLock&&) = delete;
};

// Usage:
std::mutex mtx;
void safe_increment(int& counter) {
    ScopedLock lock(mtx);
    counter++;
    // mutex automatically unlocked when lock goes out of scope
}
```

---

**C3. Fix the circular reference**

This code leaks. Fix it using `weak_ptr`:

```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;  // BUG: circular reference
    ~Node() { std::cout << "~Node\n"; }
};

void createList() {
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->next = b;
    b->prev = a;  // circular!
}
// Neither Node is ever destroyed
```

A:
```cpp
struct Node {
    std::shared_ptr<Node> next;
    std::weak_ptr<Node> prev;  // FIX: weak_ptr breaks the cycle
    ~Node() { std::cout << "~Node\n"; }
};

void createList() {
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->next = b;
    b->prev = a;  // weak_ptr — doesn't increment refcount

    // To use prev:
    if (auto p = b->prev.lock()) {
        // p is a shared_ptr to a — safe to use
    }
}
// Both Nodes are destroyed: "~Node" printed twice
```

---

**C4. Copy-and-swap idiom**

Implement a `DynamicArray` class with proper copy semantics using the copy-and-swap idiom.

A:
```cpp
class DynamicArray {
    int* data_;
    size_t size_;
public:
    DynamicArray(size_t n) : data_(new int[n]()), size_(n) {}

    ~DynamicArray() { delete[] data_; }

    // Copy constructor
    DynamicArray(const DynamicArray& other)
        : data_(new int[other.size_]), size_(other.size_) {
        std::copy(other.data_, other.data_ + size_, data_);
    }

    // Move constructor
    DynamicArray(DynamicArray&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    // Unified assignment (copy-and-swap)
    DynamicArray& operator=(DynamicArray other) {  // by value!
        swap(*this, other);
        return *this;
    }

    friend void swap(DynamicArray& a, DynamicArray& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }

    size_t size() const { return size_; }
    int& operator[](size_t i) { return data_[i]; }
};
```

The copy-and-swap idiom gives you exception-safe assignment for free: the copy is made in the parameter, and swap is noexcept.
