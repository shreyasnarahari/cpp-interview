# Memory Management & RAII — Hard Questions

---

**Q1. Explain the `shared_ptr` control block in detail.**

A: When you create a `shared_ptr`, a **control block** is allocated (either separately or alongside the object with `make_shared`). It contains:

1. **Strong reference count** (atomic) — number of `shared_ptr`s owning the object
2. **Weak reference count** (atomic) — number of `weak_ptr`s observing + 1 (for the strong count itself)
3. **Deleter** — how to destroy the object (default or custom)
4. **Allocator** — how to free the control block memory
5. **Pointer to managed object** (if allocated separately)

```
make_shared allocation (single block):
┌─────────────────────────────────────┐
│ Control Block      │ Object (T)     │
│ ┌─────────────┐    │                │
│ │ strong: 1   │    │ [data...]      │
│ │ weak:   1   │    │                │
│ │ deleter     │    │                │
│ └─────────────┘    │                │
└─────────────────────────────────────┘

shared_ptr(new T) allocation (two blocks):
┌─────────────┐     ┌──────────┐
│ Control Block│ ──→ │ Object T │
│ strong: 1   │     │ [data...] │
│ weak:   1   │     └──────────┘
│ deleter     │      ↑ freed when strong→0
└─────────────┘
 ↑ freed when weak→0
```

Key implication: with `make_shared`, the object memory is NOT freed until all `weak_ptr`s are also gone (because the control block and object share one allocation).

---

**Q2. What is placement new? When would you use it?**

A: Placement new constructs an object in pre-allocated memory without allocating:

```cpp
// Allocate raw memory
void* buffer = std::malloc(sizeof(Widget));

// Construct Widget in that memory
Widget* w = new (buffer) Widget(42);

// Must manually call destructor (NOT delete!)
w->~Widget();

// Free the raw memory
std::free(buffer);
```

Use cases:
- **Memory pools / custom allocators** — pre-allocate a large block, place objects in it
- **Aligned memory** — allocate aligned storage, construct in it
- **`std::vector` internals** — vector uses placement new to construct elements in reserved capacity
- **Embedded / real-time systems** — avoid dynamic allocation entirely

---

**Q3. What is the Pimpl idiom? How does `unique_ptr` support it?**

A: Pointer to Implementation — hide class internals behind a forward-declared implementation class:

```cpp
// widget.h — public header
class Widget {
public:
    Widget();
    ~Widget();  // must be declared (defined in .cpp)
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;
    void doWork();
private:
    struct Impl;  // forward declaration only
    std::unique_ptr<Impl> pImpl;
};

// widget.cpp — implementation file
struct Widget::Impl {
    std::string name;
    std::vector<double> data;
    HeavyDependency dep;
    // All the heavy headers are only in .cpp
};

Widget::Widget() : pImpl(std::make_unique<Impl>()) {}
Widget::~Widget() = default;  // must be HERE, where Impl is complete
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;

void Widget::doWork() { pImpl->dep.process(pImpl->data); }
```

Benefits: compilation firewall (changes to Impl don't recompile users), ABI stability.
Key: destructor MUST be defined in the .cpp file where `Impl` is complete.

---

**Q4. What are allocator-aware containers?**

A: All STL containers accept an optional allocator template parameter:

```cpp
template <class T, class Allocator = std::allocator<T>>
class vector;
```

Custom allocators let you control where memory comes from:
- **Arena/pool allocators** — fast, no fragmentation, bulk deallocation
- **Stack allocators** — for short-lived containers
- **Shared memory allocators** — for IPC
- **Tracking allocators** — for debugging/profiling

```cpp
// Simple monotonic allocator sketch
template <typename T>
struct ArenaAllocator {
    using value_type = T;
    Arena& arena;
    ArenaAllocator(Arena& a) : arena(a) {}
    T* allocate(size_t n) { return static_cast<T*>(arena.alloc(n * sizeof(T))); }
    void deallocate(T*, size_t) { /* no-op — arena frees all at once */ }
};

Arena arena(1024 * 1024);  // 1MB arena
std::vector<int, ArenaAllocator<int>> vec(ArenaAllocator<int>(arena));
```

---

## Coding Questions

---

**C1. Implement a simplified `SharedPtr<T>`**

A:
```cpp
template <typename T>
class SharedPtr {
    T* ptr_ = nullptr;
    std::atomic<int>* refcount_ = nullptr;

    void release() {
        if (refcount_) {
            (*refcount_)--;
            if (*refcount_ == 0) {
                delete ptr_;
                delete refcount_;
            }
            ptr_ = nullptr;
            refcount_ = nullptr;
        }
    }

public:
    SharedPtr() = default;

    explicit SharedPtr(T* p) : ptr_(p), refcount_(new std::atomic<int>(1)) {}

    SharedPtr(const SharedPtr& other) : ptr_(other.ptr_), refcount_(other.refcount_) {
        if (refcount_) (*refcount_)++;
    }

    SharedPtr(SharedPtr&& other) noexcept : ptr_(other.ptr_), refcount_(other.refcount_) {
        other.ptr_ = nullptr;
        other.refcount_ = nullptr;
    }

    SharedPtr& operator=(const SharedPtr& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            refcount_ = other.refcount_;
            if (refcount_) (*refcount_)++;
        }
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            refcount_ = other.refcount_;
            other.ptr_ = nullptr;
            other.refcount_ = nullptr;
        }
        return *this;
    }

    ~SharedPtr() { release(); }

    T* get() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }
    int use_count() const { return refcount_ ? *refcount_ : 0; }
    explicit operator bool() const { return ptr_ != nullptr; }
};
```

Note: real `shared_ptr` uses **atomic** refcount and separates the control block. This is a simplified teaching version.

---

**C2. Object Pool with RAII**

Implement an object pool that pre-allocates objects and returns them via a RAII handle.

A:
```cpp
template <typename T>
class ObjectPool {
    std::vector<std::unique_ptr<T>> pool_;
    std::vector<T*> available_;

public:
    ObjectPool(size_t n) {
        for (size_t i = 0; i < n; i++) {
            pool_.push_back(std::make_unique<T>());
            available_.push_back(pool_.back().get());
        }
    }

    // RAII handle — returns object to pool on destruction
    class Handle {
        ObjectPool& pool_;
        T* obj_;
    public:
        Handle(ObjectPool& pool, T* obj) : pool_(pool), obj_(obj) {}
        ~Handle() { if (obj_) pool_.available_.push_back(obj_); }
        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        Handle(Handle&& other) noexcept : pool_(other.pool_), obj_(other.obj_) {
            other.obj_ = nullptr;
        }
        T* operator->() { return obj_; }
        T& operator*() { return *obj_; }
    };

    std::optional<Handle> acquire() {
        if (available_.empty()) return std::nullopt;
        T* obj = available_.back();
        available_.pop_back();
        return Handle(*this, obj);
    }
};

// Usage:
ObjectPool<Connection> pool(10);
if (auto conn = pool.acquire()) {
    conn->query("SELECT 1");
}  // Connection automatically returned to pool
```

---

**C3. Memory-safe string class**

Implement a `String` class with proper value semantics (Rule of Five + swap).

A:
```cpp
class String {
    char* data_ = nullptr;
    size_t size_ = 0;

public:
    String() = default;

    String(const char* s) {
        if (!s) {
            size_ = 0;
            data_ = new char[1]{'\0'};
            return;
        }
        size_ = std::strlen(s);
        data_ = new char[size_ + 1];
        std::memcpy(data_, s, size_ + 1);
    }

    ~String() { delete[] data_; }

    String(const String& other) : size_(other.size_), data_(new char[other.size_ + 1]) {
        std::memcpy(data_, other.data_, size_ + 1);
    }

    String(String&& other) noexcept : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    String& operator=(String other) {  // copy-and-swap
        swap(*this, other);
        return *this;
    }

    friend void swap(String& a, String& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }

    const char* c_str() const { return data_ ? data_ : ""; }
    size_t size() const { return size_; }

    friend std::ostream& operator<<(std::ostream& os, const String& s) {
        return os << s.c_str();
    }
};
```

---

**C4. Detect all bugs**

Find every memory-related bug:

```cpp
class ResourceManager {
    int* data;
    Widget* widget;
    FILE* logfile;
public:
    ResourceManager(int n, const char* logpath)
        : data(new int[n])
        , widget(new Widget())          // if this throws, data leaks
        , logfile(fopen(logpath, "w"))   // if this fails, widget leaks
    {}
    ~ResourceManager() {
        delete data;     // wrong: should be delete[]
        delete widget;
        fclose(logfile); // crash if logfile is nullptr
    }
};
```

A: Bugs:
1. `delete data` should be `delete[] data` — allocated with `new[]`
2. If `new Widget()` throws, `data` leaks (ctor didn't complete, dtor won't run)
3. If `fopen` returns nullptr, `fclose(nullptr)` is UB
4. No copy/move operations → double delete if copied (Rule of Three violation)

Fixed version:
```cpp
class ResourceManager {
    std::vector<int> data;
    std::unique_ptr<Widget> widget;
    std::unique_ptr<FILE, decltype(&fclose)> logfile;
public:
    ResourceManager(int n, const char* logpath)
        : data(n)
        , widget(std::make_unique<Widget>())
        , logfile(fopen(logpath, "w"), fclose)
    {
        if (!logfile) throw std::runtime_error("Failed to open log");
    }
    // Rule of Zero — all members handle their own cleanup
};
```
