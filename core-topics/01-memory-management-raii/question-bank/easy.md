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

#### Key Design Decisions & Mechanics:

1. **Why `explicit` Constructor?**
   - **Prevents Accidental Implicit Conversions**: Constructors callable with a single argument act as implicit type-conversion operators by default.
   - Without `explicit`, a function `void process(FileHandle fh)` could be invoked passing a raw string: `process("data.txt")`. The compiler would silently construct a temporary `FileHandle`, perform disk I/O, execute `process`, and immediately close the file when the temporary destructs at the end of the statement. Marking the constructor `explicit` forces callers to write `FileHandle("data.txt")`, preventing hidden resource acquisition.

2. **Why Delete Copy Operations (`= delete`)?**
   - **Exclusive Resource Ownership**: A `FILE*` stream represents exclusive ownership of an OS file descriptor.
   - If copying were permitted (the compiler-generated shallow copy), two `FileHandle` instances would hold the identical raw `FILE*`. When the first instance goes out of scope, its destructor calls `fclose(file_)`. When the second instance goes out of scope, it calls `fclose(file_)` on an already closed/invalidated pointer, causing a **Double Close Bug (Undefined Behavior)** and memory corruption if the OS has already reallocated that file descriptor number to another thread!

3. **Why `noexcept` on Move Constructor & Move Assignment?**
   - **Inherent Invariant**: Transferring ownership of a raw pointer is purely bitwise assignment (`file_ = other.file_`), which can never fail or throw an exception.
   - **`std::vector` Reallocation (`std::move_if_noexcept`)**: When `std::vector<FileHandle>` grows and reallocates memory, it inspects `is_nothrow_move_constructible_v<T>`. If the move constructor is not marked `noexcept`, `std::vector` falls back to copying elements to maintain the Strong Exception Guarantee. Since copy operations are deleted, omitting `noexcept` causes `std::vector` operations to fail to compile!

4. **Why Null-Check in Destructor (`if (file_) fclose(file_)`)?**
   - **Moved-From State Safety**: When an object is moved (`std::move(fh)`), the source object's pointer is cleared to `nullptr` (`other.file_ = nullptr`).
   - The moved-from object still resides on the stack, and its destructor `~FileHandle()` will execute when its scope exits. Passing `nullptr` to C standard library `std::fclose(NULL)` is **Undefined Behavior** (causes an immediate `SIGSEGV` on Linux/glibc). The null-check guarantees that moved-from instances destruct cleanly as a safe no-op.

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

// Approach 1: Function Pointer with decltype
auto db = std::unique_ptr<DatabaseHandle, decltype(&db_close)>(
    db_open("mydb.sqlite"),
    db_close
);
// db_close is called automatically when db goes out of scope
```

Or with a stateless struct functor (Optimal — **Zero Memory Overhead**):
```cpp
// Approach 2: Stateless Struct Deleter (Recommended Production Idiom)
struct DbDeleter {
    void operator()(DatabaseHandle* handle) const noexcept {
        if (handle) db_close(handle);
    }
};

using UniqueDbHandle = std::unique_ptr<DatabaseHandle, DbDeleter>;

UniqueDbHandle db(db_open("mydb.sqlite")); 
// No need to pass deleter instance in constructor! sizeof(db) is exactly 8 bytes!
```

Or with a lambda (C++20):
```cpp
// Approach 3: Stateless Lambda Deleter
auto db_deleter = [](DatabaseHandle* h) noexcept { if (h) db_close(h); };
std::unique_ptr<DatabaseHandle, decltype(db_deleter)> db(db_open("mydb.sqlite"));
```

#### Detailed Syntax, Mechanics & Operational Breakdown:

1. **The Two-Argument Template Signature: `std::unique_ptr<T, Deleter>`**
   - Unlike raw pointers, `std::unique_ptr` accepts two template parameters:
     1. `T`: The managed object type (`DatabaseHandle`).
     2. `Deleter`: The callable type responsible for destroying the resource (defaults to `std::default_delete<T>`, which runs `delete ptr`).
   - **Critical Architectural Difference from `std::shared_ptr`**: In `std::shared_ptr`, the deleter is *type-erased* into the runtime control block on the heap. In `std::unique_ptr`, the deleter type is **baked directly into the static C++ type** at compile-time, allowing the compiler to inline the destruction call completely.

2. **Syntax Breakdown: `decltype(&db_close)` vs Struct Functor**
   - `&db_close` extracts the memory address of the C function.
   - `decltype(&db_close)` deduces the function pointer type: `void (*)(DatabaseHandle*)`.
   - **Constructor Requirement**: When using a function pointer deleter, you **must pass the function pointer as the second constructor argument** (`db_close`), because function pointers cannot be default-constructed.
   - **Memory Overhead Comparison**:
     | Deleter Technique | `sizeof(unique_ptr)` | Why? |
     |---|---|---|
     | **Function Pointer (`decltype(&db_close)`)** | **16 Bytes** | Stores 8B raw handle pointer + 8B function pointer. |
     | **Stateless Functor (`struct DbDeleter`)** | **8 Bytes** (Zero overhead!) | Exploits **Empty Base Optimization (EBO)** / `[[no_unique_address]]` to eliminate empty struct size. |
     | **C++20 Stateless Lambda** | **8 Bytes** (Zero overhead!) | C++20 stateless lambdas are default-constructible and zero-sized. |

3. **Destruction & Operational Lifecycle**
   - **Destructor Invocation**: When `db` goes out of scope (or when `db.reset()` is called), `std::unique_ptr` checks `if (ptr != nullptr) get_deleter()(ptr);`.
   - **Exception Safety**: If an exception is thrown anywhere after `db` is created, stack unwinding automatically calls `db_close`, preventing handle leaks without requiring `try/catch`.
   - **Ownership Transfer**: Supports full move semantics:
     ```cpp
     UniqueDbHandle db2 = std::move(db); // Transfers ownership; db becomes nullptr
     ```
   - **Manual Early Release**:
     - `db.reset()`: Explicitly closes the handle immediately and resets to `nullptr`.
     - `DatabaseHandle* raw = db.release()`: Relinquishes ownership without closing the handle (caller becomes responsible for manual `db_close(raw)`).

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
