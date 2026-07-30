# Theory & Mechanics: Memory Management & RAII

## 1. Stack vs Heap — Under the Hood

**Stack** allocation is a single instruction — decrementing the stack pointer (`sub rsp, N`). Deallocation is the reverse (`add rsp, N`). The OS pre-allocates a fixed stack (typically 1–8 MB per thread). Exceeding it triggers a **stack overflow** (segfault).

**Heap** allocation (`new` → `malloc`) is non-trivial:
1. The allocator searches its **free list** for a suitable block (first-fit, best-fit, or slab).
2. If no block exists, the allocator requests memory from the kernel via `brk()` (extend data segment) or `mmap()` (map new pages) — both are **syscalls** (~1000 cycles).
3. The returned virtual pages may not be backed by physical memory until first access (**page fault**).

```
Stack Frame Layout (x86-64):
┌─────────────────────────┐ ← High address
│ Caller's frame          │
├─────────────────────────┤ ← RBP (frame pointer)
│ Saved RBP               │
│ Return address          │
│ Local variable 1        │
│ Local variable 2        │
│ ...                     │
└─────────────────────────┘ ← RSP (stack pointer, low address)
```

**Interview key point:** Stack allocation is O(1) with zero syscall cost. Heap allocation has non-deterministic latency due to potential syscalls. This is why HFT systems pre-allocate.

## 2. `std::shared_ptr` Control Block Layout

When you create a `shared_ptr`, a **control block** is heap-allocated alongside (or combined with) the managed object:

```
shared_ptr<T> sp(new T);  — TWO separate allocations:

  shared_ptr object:          Control Block:             T object:
  ┌──────────────┐            ┌──────────────────┐       ┌──────────┐
  │ T* ptr  ─────────────────────────────────────────────→│ T data   │
  │ CtrlBlk* ────────────────→│ strong_count: 1  │       └──────────┘
  └──────────────┘            │ weak_count:   1  │
                              │ deleter          │
                              │ allocator        │
                              └──────────────────┘

make_shared<T>(args) — SINGLE allocation (object embedded in control block):

  shared_ptr object:          Control Block + T:
  ┌──────────────┐            ┌──────────────────┐
  │ T* ptr  ─────────────────→│ strong_count: 1  │
  │ CtrlBlk* ────────────────→│ weak_count:   1  │
  └──────────────┘            │ deleter          │
                              │ allocator        │
                              │ T data (in-place)│
                              └──────────────────┘
```

### Why `make_shared` is preferred:
1. **Single allocation** — one `malloc` call instead of two (fewer syscalls, better cache locality).
2. **Exception safety** — `f(shared_ptr<A>(new A), shared_ptr<B>(new B))` can leak if `new A` succeeds but `shared_ptr<B>` construction throws. `make_shared` is atomic.
3. **Trade-off:** Memory for `T` is not freed until all `weak_ptr`s are gone (the control block and object share the same allocation).

### Reference counting is atomic:
```cpp
// Incrementing the reference count uses atomic operations:
strong_count.fetch_add(1, std::memory_order_relaxed);   // copy

// Decrementing requires acquire/release:
if (strong_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    delete ptr;  // last owner — destroy the object
}
```

This makes `shared_ptr` copies **thread-safe for the refcount**, but NOT thread-safe for the pointed-to object itself.

## 3. RAII — Deterministic Destruction Ordering

RAII guarantees resources are released in **reverse order of acquisition** — the stack unwinds from top to bottom.

```cpp
void process() {
    std::lock_guard<std::mutex> lock(mtx);    // acquired 1st
    auto file = std::make_unique<File>("a");   // acquired 2nd
    auto conn = std::make_unique<DBConn>();    // acquired 3rd
    // ...
}   // destruction order: conn → file → lock (reverse of construction)
```

**Class member destruction order:** Members are destroyed in **reverse declaration order** (not reverse initializer list order — this is a common trap):

```cpp
class Widget {
    A a;   // destroyed 3rd (declared 1st)
    B b;   // destroyed 2nd
    C c;   // destroyed 1st (declared last)
};
```

## 4. Custom Deleters

`unique_ptr` with a custom deleter has **zero overhead** only if the deleter is a stateless functor or lambda:

```cpp
// Zero overhead — stateless lambda:
auto file = std::unique_ptr<FILE, decltype(&fclose)>(fopen("f.txt", "r"), fclose);
// sizeof(file) == 2 * sizeof(void*)  — pointer + function pointer

// Zero overhead — stateless functor:
struct FileCloser { void operator()(FILE* f) { fclose(f); } };
auto file2 = std::unique_ptr<FILE, FileCloser>(fopen("f.txt", "r"));
// sizeof(file2) == sizeof(void*)  — EBO eliminates empty functor

// Overhead — std::function (heap allocation + type erasure):
std::shared_ptr<FILE> file3(fopen("f.txt", "r"), fclose);
// Deleter stored in control block — no sizeof increase for shared_ptr itself
```
