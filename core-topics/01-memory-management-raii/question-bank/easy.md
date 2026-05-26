# Memory Management & RAII — Easy Questions

Try to answer each question before reading the answer.

---

**Q1. What is RAII?**

A: **Resource Acquisition Is Initialization.** A C++ idiom where resource lifetime is tied to object lifetime:
- Constructor acquires the resource
- Destructor releases the resource
- Stack unwinding guarantees cleanup even during exceptions

```cpp
class FileHandle {
    FILE* f;
public:
    FileHandle(const char* path) : f(fopen(path, "r")) {}
    ~FileHandle() { if (f) fclose(f); }
};
// f is automatically closed when FileHandle goes out of scope
```

RAII is the foundation of exception-safe C++. It eliminates the need for `try/finally` (which C++ doesn't have) and manual cleanup.

---

**Q2. What is the difference between stack and heap allocation?**

A:

| | Stack | Heap |
|---|---|---|
| Speed | Very fast (pointer bump) | Slower (allocator overhead) |
| Size | Limited (~1-8MB) | Limited only by system memory |
| Lifetime | Automatic (scope-based) | Manual (`new`/`delete`) or smart pointer |
| Fragmentation | None | Can fragment over time |
| Thread safety | Each thread has own stack | Shared, allocator must synchronize |

```cpp
void f() {
    int x = 42;              // stack — destroyed at end of f()
    int* p = new int(42);    // heap — lives until delete p
    delete p;
}
```

---

**Q3. What is `std::unique_ptr`?**

A: A smart pointer that owns a dynamically allocated object **exclusively**. It cannot be copied, only moved. When the `unique_ptr` goes out of scope, it `delete`s the managed object.

```cpp
auto p = std::make_unique<int>(42);  // p owns the int
// auto p2 = p;                      // ERROR — cannot copy
auto p2 = std::move(p);             // OK — p is now nullptr, p2 owns it
// p goes out of scope — nothing happens (nullptr)
// p2 goes out of scope — deletes the int
```

Zero overhead compared to raw `new`/`delete` — no control block, no reference counting.

---

**Q4. What is the Rule of Zero?**

A: If your class doesn't directly manage a resource (it uses smart pointers, `std::string`, `std::vector`, etc.), **don't declare any special member functions**. Let the compiler generate them.

```cpp
// Rule of Zero — correct, simple, exception-safe
class Employee {
    std::string name;
    std::vector<std::string> skills;
    std::unique_ptr<Badge> badge;
    // No destructor, no copy/move operations needed!
    // Compiler generates correct ones automatically.
};
```

The Rule of Zero is the modern default. You only need Rule of Five when you directly manage raw resources (rare).

---

**Q5. What happens if you delete a nullptr?**

A: Nothing — it's a guaranteed no-op. The C++ standard explicitly says `delete nullptr` and `delete[] nullptr` are safe.

```cpp
int* p = nullptr;
delete p;    // safe — no-op
delete[] p;  // safe — no-op
```

This is why destructors and smart pointers don't need to check for null before deleting.

---

## Coding Questions

---

**C1. RAII File Handle**

Write a class `FileHandle` that wraps `FILE*` using RAII. It should support move but not copy.

A:
```cpp
class FileHandle {
    FILE* file_ = nullptr;
public:
    explicit FileHandle(const char* path, const char* mode = "r")
        : file_(std::fopen(path, mode)) {
        if (!file_) throw std::runtime_error("Failed to open file");
    }

    ~FileHandle() {
        if (file_) std::fclose(file_);
    }

    // Non-copyable
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    // Movable
    FileHandle(FileHandle&& other) noexcept : file_(other.file_) {
        other.file_ = nullptr;
    }

    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            if (file_) std::fclose(file_);
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }

    FILE* get() const { return file_; }
};
```

Key points: explicit constructor, delete copy, noexcept on move, null-check in destructor.

---

**C2. Fix the leak**

Fix this code so it never leaks:

```cpp
void processData() {
    int* data = new int[1000];
    std::string result = transform(data);  // might throw!
    delete[] data;
}
```

A:
```cpp
void processData() {
    auto data = std::make_unique<int[]>(1000);
    std::string result = transform(data.get());  // if this throws, data is still freed
    // no manual delete needed
}
```

Or even better — use `std::vector<int>`:
```cpp
void processData() {
    std::vector<int> data(1000);
    std::string result = transform(data.data());
}
```

---

**C3. unique_ptr for C library handle**

Write a custom-deleter unique_ptr to manage a hypothetical `DatabaseHandle*` with `db_open()` / `db_close()` C API.

A:
```cpp
// C API (given)
// DatabaseHandle* db_open(const char* path);
// void db_close(DatabaseHandle* handle);

auto db = std::unique_ptr<DatabaseHandle, decltype(&db_close)>(
    db_open("mydb.sqlite"),
    db_close
);
// db_close is called automatically when db goes out of scope
```

Or with a lambda:
```cpp
auto db = std::unique_ptr<DatabaseHandle, void(*)(DatabaseHandle*)>(
    db_open("mydb.sqlite"),
    [](DatabaseHandle* h) { db_close(h); }
);
```

---

**C4. Spot all the bugs**

Find and fix every bug in this code:

```cpp
class Buffer {
    int* data;
    size_t size;
public:
    Buffer(size_t n) : data(new int[n]), size(n) {}
    ~Buffer() { delete data; }
};
```

A: Three bugs:
1. `delete data` should be `delete[] data` — allocated with `new[]`
2. Missing copy constructor → double delete if copied
3. Missing copy assignment → same problem

Fixed (Rule of Five or Rule of Zero):
```cpp
class Buffer {
    std::vector<int> data;  // Rule of Zero — let vector manage memory
public:
    Buffer(size_t n) : data(n) {}
    // Compiler-generated copy, move, destructor all work correctly
};
```
