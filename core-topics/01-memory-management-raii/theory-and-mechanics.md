# Memory Management & RAII — Deep Theory & Mechanics

An exhaustive, engineering-grade reference on C++ memory architecture, the memory model, physical & virtual address spaces, dynamic allocation internals, RAII, smart pointer mechanics, allocator design, and memory corruption diagnosis.

---

## 1. Memory Architecture & Address Space Layout

### 1.1 Process Virtual Address Space (x86-64 Linux)

Every 64-bit user-space process runs within its own virtual address space isolated by the OS kernel and CPU Memory Management Unit (MMU) with 4-level page tables (PML4).

```
+-------------------------------------------------------------------+ 0x7FFFFFFFFFFF (Canonical User Space Limit)
| STACK (Grows Downward <-)                                         |
|  - Stack frames: Return addresses, saved %rbp, local variables   |
|  - Bounded size (~1MB to 8MB; `ulimit -s`)                        |
+-------------------------------------------------------------------+
|                           |                                       |
|                           v (Downwards)                           |
|                                                                   |
|                           ^ (Upwards)                             |
|                           |                                       |
+-------------------------------------------------------------------+
| MEMORY MAPPING SEGMENT (`mmap`)                                   |
|  - Shared libraries (.so), dynamic allocations > MMAP_THRESHOLD   |
|  - Memory-mapped files, anonymous memory pages                    |
+-------------------------------------------------------------------+
| HEAP (Grows Upward -> via `brk`/`sbrk`)                           |
|  - Managed by allocator (`glibc ptmalloc`, `jemalloc`, `tcmalloc`)|
+-------------------------------------------------------------------+
| BSS SEGMENT (Uninitialized Global/Static Variables)               |
|  - Zero-filled on demand by OS kernel (Copy-on-Write zero page)   |
+-------------------------------------------------------------------+
| DATA SEGMENT (Initialized Global/Static Variables)                |
|  - Explicitly initialized data loaded directly from ELF binary    |
+-------------------------------------------------------------------+
| TEXT SEGMENT (Executable Machine Code & `.rodata`)                |
|  - Read-Only & Executable (RX); string literals, jump tables      |
+-------------------------------------------------------------------+ 0x0000000000400000
| RESERVED NULL PAGE (Traps nullptr dereferences via Page Fault)    |
+-------------------------------------------------------------------+ 0x0000000000000000
```

### 1.2 Stack vs. Heap: Physical Mechanics

| Metric | Stack Allocation | Heap Allocation |
|---|---|---|
| **Allocation Mechanism** | Pointer subtraction on `%rsp` register (Single CPU instruction: `sub %rsp, N`). | Dynamic allocator metadata lookup (`ptmalloc` bins, chunk splitting, locks, or `mmap`/`brk` syscalls). |
| **Allocation Latency** | **$< 1\text{ ns}$** (Deterministic, near-instantaneous). | **$10\text{ ns} - 100+\text{ ns}$** (Non-deterministic; cache misses, lock contention, page faults). |
| **Deallocation** | Pointer addition on `%rsp` register (`add %rsp, N`). | Free-list coalescing, chunk insertion, potential OS unmapping (`madvise`/`munmap`). |
| **Lifetime** | Automatic / Lexical scope-bound. | Explicit / Programmatic (until `delete`, `free`, or refcount reach zero). |
| **Cache Locality** | Extremely high (hot top-of-stack in L1 CPU cache). | Variable; vulnerable to spatial fragmentation and pointer chasing. |
| **Thread Safety** | Thread-local private stack (zero contention). | Global shared heap (requires per-thread arenas or mutex synchronization). |
| **Capacity** | Fixed limit ($1 - 8\text{ MB}$; overflow triggers `SIGSEGV`). | Bound only by physical RAM + OS Swap space. |

---

## 2. Dynamic Memory Primitives: C vs C++

### 2.1 `malloc`/`free` vs `new`/`delete`

```
                    +-------------------------------------------------+
                    | `new T(args...)` Expression                     |
                    +-------------------------------------------------+
                                            |
                         1. Allocate Memory | (Calls `operator new(sizeof(T))`)
                                            v
                    +-------------------------------------------------+
                    | Raw Heap Chunk (Uninitialized Memory)           |
                    +-------------------------------------------------+
                                            |
                       2. Construct Object  | (Invokes `T::T(args...)` in-place)
                                            v
                    +-------------------------------------------------+
                    | Fully Initialized Object at Typed Address       |
                    +-------------------------------------------------+
```

- **`malloc(size_t)`**: Allocates raw, unformatted bytes. Does **NOT** invoke constructors. Returns `void*` or `NULL` on failure.
- **`new T`**: Two-stage operation:
  1. Allocates raw memory via `operator new(sizeof(T))`.
  2. Constructs object in-place by calling `T::T(...)`.
  3. Throws `std::bad_alloc` on memory exhaustion (unless `std::nothrow` is passed: `new (std::nothrow) T`).
- **`new[]` vs `delete[]` Array Suffix Mechanics**:
  - `new T[N]` allocates memory for $N$ objects **plus an allocator cookie header** (usually 8 bytes on 64-bit platforms) storing the array size $N$.
  - `delete[] ptr` reads the size cookie immediately preceding `ptr`, calls the destructor `~T()` on all $N$ elements in reverse order, and frees the entire block.
  - **Fatal UB Trap**: Invoking `delete ptr` on memory allocated with `new[]` skips element destructors and miscalculates the base heap pointer, causing heap corruption!

### 2.2 Placement `new` and Explicit Destructor Invocation

Placement `new` constructs an object within a pre-allocated memory buffer without requesting heap memory from the OS allocator:

```cpp
alignas(alignof(Widget)) uint8_t storage[sizeof(Widget)];

// 1. Placement new: Constructs Widget directly in 'storage'
Widget* w = new (storage) Widget("AAPL", 150.0);

// 2. Usage
w->process();

// 3. Explicit Destructor: MANDATORY (do NOT call delete w!)
w->~Widget();
```

---

## 3. RAII & The Exception-Safety Guarantees

### 3.1 Resource Acquisition Is Initialization (RAII)
RAII binds the lifecycle of a resource (heap memory, file descriptors, socket handles, mutex locks, database transactions) to the lifetime of an automatic (stack-allocated) object.

```
Scope Entry -> Constructor Acquires Resource
     |
  [Normal Execution OR Exception Thrown]
     |
Stack Unwinding (Automatic, Deterministic) -> Destructor Releases Resource
```

### 3.2 The Four Exception Safety Levels

1. **Nothrow (No-fail) Guarantee**: The function is guaranteed never to throw an exception under any circumstances (`noexcept`). Mandatory for destructors, move operations, and swap routines.
2. **Strong Exception Guarantee (Commit-or-Rollback)**: If an exception is thrown, program state remains exactly as it was before the function call (no partial state changes).
3. **Basic Exception Guarantee**: If an exception is thrown, no resources or memory are leaked, and all objects remain in an internally consistent, destructible state.
4. **No Guarantee**: If an exception occurs, memory is leaked, invariants are violated, or undefined behavior ensues.

### 3.3 The Copy-and-Swap Idiom
Achieves the **Strong Exception Guarantee** and eliminates code duplication between copy constructors and copy assignment operators:

```cpp
class DynamicArray {
    size_t size_{0};
    int* data_{nullptr};

public:
    DynamicArray(size_t size) : size_(size), data_(new int[size]()) {}

    ~DynamicArray() noexcept { delete[] data_; }

    // Copy Constructor
    DynamicArray(const DynamicArray& other) 
        : size_(other.size_), data_(other.size_ ? new int[other.size_]() : nullptr) {
        std::copy(other.data_, other.data_ + other.size_, data_);
    }

    // Move Constructor
    DynamicArray(DynamicArray&& other) noexcept 
        : size_(std::exchange(other.size_, 0)), data_(std::exchange(other.data_, nullptr)) {}

    // Unified Assignment Operator (Passed by Value -> Copy constructed or Move constructed)
    DynamicArray& operator=(DynamicArray other) noexcept {
        swap(*this, other);
        return *this;
    }

    friend void swap(DynamicArray& first, DynamicArray& second) noexcept {
        using std::swap;
        swap(first.size_, second.size_);
        swap(first.data_, second.data_);
    }
};
```

---

## 4. Special Member Functions & The Rules of Ownership

```
                            RESOURCE MANAGEMENT RULES
                                        |
                   Does your class directly manage a raw resource?
                                  /            \
                                 /              \
                               YES               NO
                               /                  \
                    +--------------------+    +--------------------+
                    | RULE OF FIVE /     |    |   RULE OF ZERO     |
                    | RULE OF THREE      |    | (Default / Best)   |
                    +--------------------+    +--------------------+
                    | Explicitly declare:|    | Use smart pointers |
                    | 1. Destructor      |    | & STL containers.  |
                    | 2. Copy Ctor       |    | Declare ZERO       |
                    | 3. Copy Assign     |    | special member     |
                    | 4. Move Ctor       |    | functions.         |
                    | 5. Move Assign     |    +--------------------+
                    +--------------------+
```

---

## 5. Smart Pointers: Internal Mechanics & Control Blocks

### 5.1 `std::unique_ptr<T, Deleter>`

- **Memory Overhead**: Exactly `sizeof(T*)` (8 bytes) when using stateless deleters (e.g., default `std::default_delete<T>`) via Empty Base Optimization (EBO) / `[[no_unique_address]]`.
- **Custom Deleters**:
  - Stateless functor: `sizeof(unique_ptr) == 8` bytes.
  - Function pointer: `sizeof(unique_ptr) == 16` bytes (stores pointer to object + pointer to function).
- **Array Specialization**: `std::unique_ptr<T[]>` automatically invokes `delete[]` and provides `operator[]`.

### 5.2 `std::shared_ptr<T>` & Control Block Architecture

A `std::shared_ptr` consists of **two raw 64-bit pointers** (16 bytes total):
1. Pointer to the managed object (`T*`).
2. Pointer to the reference-counted **Control Block**.

```
std::shared_ptr<Widget> (16 Bytes)
+-------------------+-------------------+
|  T* ptr           | ControlBlock* cb  |
+---------|---------+---------|---------+
          |                   |
          v                   v
   +--------------+    +-----------------------------------------------+
   | Widget Data  |    | CONTROL BLOCK                                 |
   |              |    |  - std::atomic<long> strong_ref_count (Use)   |
   |              |    |  - std::atomic<long> weak_ref_count   (Refs)  |
   |              |    |  - Custom Deleter (Type-Erased)               |
   |              |    |  - Custom Allocator (Type-Erased)             |
   |              |    |  - [Optional] Inlined Widget (if make_shared) |
   +--------------+    +-----------------------------------------------+
```

#### `std::make_shared` vs `std::shared_ptr<T>(new T)`

```
std::shared_ptr<T>(new T) -> 2 SEPARATE HEAP ALLOCATIONS:
  [ Heap Block 1: T Data (e.g., 32B) ]  <--->  [ Heap Block 2: Control Block (48B) ]

std::make_shared<T>() -> 1 SINGLE COMBINED HEAP ALLOCATION (Cache-Friendly):
  +-----------------------------------+------------------------------------+
  | Control Block Header (16-24B)     | Inlined T Object Data (e.g., 32B)  |
  +-----------------------------------+------------------------------------+
  <---------------------- Single 64-Byte Cache Line ---------------------->
```

**Tradeoffs of `std::make_shared`**:
- **Pros**: Single allocation (faster, less heap fragmentation, zero memory overhead between blocks, cache friendly).
- **Cons / Caveats**:
  1. Cannot specify custom deleters.
  2. **Deferred Memory Deallocation**: The memory block holding `T` cannot be freed until `weak_ref_count` drops to 0, because `T` and the control block share a single continuous heap buffer! (Although `~T()` runs when `strong_ref_count == 0`, the raw memory remains allocated until all `weak_ptr`s expire).

### 5.3 `std::weak_ptr<T>` & Breaking Circular Reference Cycles

```
Circular Reference Leak (strong_ref = 2 -> Never reaches 0):
  [ Node A (shared_ptr) ] --------points to-------> [ Node B (shared_ptr) ]
         ^                                                 |
         +-------------------points to---------------------+

Breaking Cycle with std::weak_ptr:
  [ Node A (shared_ptr) ] --------points to-------> [ Node B (shared_ptr) ]
         ^                                                 |
         +..............weak_ptr (no strong ref)...........+
```

To access the object from a `std::weak_ptr<T>`:
```cpp
if (auto shared = weak_node.lock()) {
    shared->execute(); // Safe: strong ref count atomically incremented
} else {
    // Target object has already been destroyed
}
```

### 5.4 `std::enable_shared_from_this<T>`

Enables an object managed by a `shared_ptr` to safely generate additional `shared_ptr` instances from its `this` pointer:

```cpp
class Worker : public std::enable_shared_from_this<Worker> {
public:
    void register_with_manager(Manager& mgr) {
        // Safe: Reuses the existing control block!
        mgr.add_worker(shared_from_this()); 
    }
};

// FATAL TRAP: Calling shared_from_this() in a constructor
Worker::Worker() {
    // UB / throws std::bad_weak_ptr! The control block is only initialized
    // AFTER the constructor finishes when the owning shared_ptr takes ownership!
}
```

---

## 6. Custom Memory Allocators: Arenas & Pools

### 6.1 Monotonic Arena (Bump-Pointer Allocator)

```
+-------------------------------------------------------------------------------+
| MONOTONIC ARENA (Fixed Buffer: 64 KB)                                         |
|                                                                               |
| [ Object 1 (32B) ] [ Object 2 (64B) ] [ Object 3 (128B) ] ... [ FREE SPACE ] |
|                                                                 ^             |
|                                                                 | offset_ptr  |
+-------------------------------------------------------------------------------+
```

- **Allocation**: Pointer arithmetic (`offset_ptr += aligned_size`). Complexity: **$O(1)$** ($<1\text{ ns}$).
- **Deallocation**: Global reset (`offset_ptr = 0`). Individual elements are not freed. Ideal for per-frame or per-request workloads.

---

## 7. Memory Corruption Pitfalls & Sanitizer Diagnostics

| Memory Bug | Root Cause | Detection Tool |
|---|---|---|
| **Use-After-Free (UAF)** | Accessing pointer after `delete`/`free`. | AddressSanitizer (`-fsanitize=address`) |
| **Double Free** | Calling `delete` twice on same pointer. | AddressSanitizer |
| **Buffer Overflow (Stack/Heap)** | Writing past allocated boundary. | AddressSanitizer (Redzones) |
| **Memory Leak** | Losing pointer without calling `delete`. | LeakSanitizer (`ASAN_OPTIONS=detect_leaks=1`) |
| **Uninitialized Read** | Reading variable before write. | MemorySanitizer (`-fsanitize=memory`) |
| **Data Race** | Concurrent unsynchronized read/write. | ThreadSanitizer (`-fsanitize=thread`) |
