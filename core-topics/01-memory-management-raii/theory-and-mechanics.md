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

#### Detailed Breakdown of Address Space Segments:

1. **Reserved Null Page (`0x0000000000000000` to `0x0000000000400000`)**:
   - **Protection / Permissions**: None (`---`, No Read, No Write, No Execute).
   - **Purpose & Mechanics**: The lowest page (or multiple pages) of virtual memory is intentionally left unmapped by the operating system. Any attempt to read or write through a `nullptr` (address `0x0`) or a struct offset from null (e.g. `ptr->member` at `0x0 + 0x18`) immediately triggers a hardware Memory Management Unit (MMU) exception $\to$ CPU Page Fault $\to$ Linux kernel trap $\to$ `SIGSEGV` signal sent to the process. This prevents silent memory corruption.

2. **Text / Code Segment (`.text` & `.rodata`)**:
   - **Protection / Permissions**: Read-Only & Executable (`R-X`) for `.text`; Read-Only (`R--`) for `.rodata`. Write permissions are strictly stripped (`W` flag disabled) to prevent self-modifying code vulnerabilities.
   - **`.text` Subsegment**: Contains the raw compiled binary machine code instructions executed by the CPU. Multiple instances of the same binary share the exact same physical `.text` pages in RAM.
   - **`.rodata` Subsegment**: Contains immutable constant data, including:
     - String literals (e.g., `"Hello World"`).
     - Virtual Function Tables (`vtable` pointers and RTTI descriptors).
     - Switch jump tables and constant floating-point/SIMD masks.
     - *Attempting to modify data in `.rodata` (such as `char* s = "text"; s[0] = 'T';`) triggers an instant `SIGSEGV` page fault!*

3. **Data Segment (`.data`)**:
   - **Protection / Permissions**: Read & Write (`RW-`, Non-Executable).
   - **Purpose & Mechanics**: Stores global, namespace-scope, and `static` variables that are **explicitly initialized with non-zero values** at compile-time (e.g., `int g_counter = 42;`).
   - **Lifecycle**: Allocated and populated directly from the ELF executable binary file by the OS loader during process startup (`execve`). Persists for the entire duration of the process.

4. **BSS Segment (`.bss` — "Block Started by Symbol")**:
   - **Protection / Permissions**: Read & Write (`RW-`, Non-Executable).
   - **Purpose & Mechanics**: Stores global and `static` variables that are **uninitialized** (e.g., `int g_buffer[100000];`) or **explicitly initialized to zero** (e.g., `static bool s_ready = false;`).
   - **ELF Binary Size Optimization**: Does **NOT** occupy physical space inside the binary file on disk! The ELF header merely records the total required byte size. When the program is launched, the kernel allocates anonymous memory pages backed by the system's shared Copy-on-Write (CoW) Zero Page, ensuring all BSS variables are initialized to `0` with near-zero startup cost.

5. **Heap Segment (Managed via `brk` / `sbrk`)**:
   - **Protection / Permissions**: Read & Write (`RW-`, Non-Executable).
   - **Purpose & Mechanics**: Dynamic memory allocated at runtime via C++ `new`/`delete` or C `malloc()`/`free()`.
   - **Growth**: Historically grows upward towards higher addresses by moving the process "program break" pointer via the `brk()` / `sbrk()` system calls. Managed in user space by general-purpose allocators (`glibc ptmalloc`, `jemalloc`, `tcmalloc`) which partition raw heap arenas into small-object chunks and bins to minimize fragmentation.

6. **Memory Mapping Segment (`mmap`)**:
   - **Protection / Permissions**: Configurable per mapping (`PROT_READ`, `PROT_WRITE`, `PROT_EXEC`).
   - **Purpose & Mechanics**:
     - **Dynamic Shared Libraries (`.so` / DLLs)**: Code and data pages of dynamic libraries (`libc.so`, `libpthread.so`) are mapped here by the dynamic linker (`ld-linux.so`).
     - **Large Heap Allocations**: Allocations exceeding the allocator's threshold (e.g. `MMAP_THRESHOLD` $\ge 128\text{ KB}$ in `glibc`) bypass the `brk` heap and are allocated directly via anonymous `mmap()` syscalls. When freed, `munmap()` releases the pages immediately back to the OS kernel.
     - **Memory-Mapped Files**: File I/O via `mmap()` maps disk file blocks directly to virtual addresses, enabling zero-copy access.
     - **POSIX Shared Memory**: Inter-process communication regions (`shm_open`, `/dev/shm`).

7. **Stack Segment**:
   - **Protection / Permissions**: Read & Write (`RW-`, Non-Executable with hardware NX/XD bit active to prevent stack-based code injection attacks).
   - **Purpose & Mechanics**: Grows **downward** from high memory addresses toward lower addresses.
   - **Stack Frames**: Each active function invocation pushes a stack frame containing:
     - Return instruction pointer (`%rip`).
     - Saved caller frame pointer (`%rbp`).
     - Local automatic variables and RAII object instances.
     - Spilled registers and function arguments beyond the first 6 registers (`%rdi, %rsi, %rdx, %rcx, %r8, %r9` in System V AMD64 ABI).
   - **Stack Bounds & Guard Pages**: The stack size is bounded (default $8\text{ MB}$ on Linux; configured via `ulimit -s`). The OS kernel places an unmapped **Guard Page** directly below the stack. If unbounded recursion or huge stack allocations exceed the limit, the CPU hits the guard page, triggering a Page Fault $\to$ `SIGSEGV` (Stack Overflow).

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

#### Detailed Systems Terminology & Mechanics Breakdown:

#### 1. What is the `%rsp` Register & Stack Pointer Arithmetic?
- **What is a CPU Register?**: Registers are tiny, ultra-fast memory storage locations built directly inside the CPU core. Accessing a register takes **0 to 1 CPU clock cycle** ($< 0.3\text{ nanoseconds}$).
- **What does `%rsp` mean?**: In 64-bit x86 architecture (x86-64), **`%rsp`** stands for **R**egister **S**tack **P**ointer. It holds the 64-bit virtual memory address representing the **exact current top of the stack**.
- **Why is Stack Allocation Subtraction (`sub %rsp, N`)?**: 
  - Because the stack grows **downward** in memory (from high numerical addresses to low numerical addresses).
  - When a function begins and requests 32 bytes for local variables, the compiler emits a single machine instruction:
    ```assembly
    sub rsp, 32    # Moves stack boundary down by 32 bytes
    ```
  - There are no hash table lookups, no mutex locks, and no OS syscalls. It executes in $< 1\text{ ns}$.
- **Why is Stack Deallocation Addition (`add %rsp, N`)?**: 
  - When the function returns, `add rsp, 32` instantly restores the pointer back to its previous position, destroying all local variables in a single clock cycle.

---

#### 2. What is `ptmalloc`, Allocator "Bins", and Chunk Splitting?
When your program calls `new` or `malloc(100)`, the operating system is **not** invoked directly for individual small requests (the OS kernel only allocates memory in huge $4,096\text{ byte}$ units called "Pages"). Instead, an intermediary user-space library program called a **Memory Allocator** manages heap memory:

```
+-------------------------------------------------------------------------------+
| Application Layer: C++ Code (`new Widget()`, `malloc(64)`)                    |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| User-Space Allocator (`ptmalloc` in glibc / `jemalloc` / `tcmalloc`)          |
|  - FastBins / SmallBins / LargeBins / Unsorted Bin                            |
|  - Splitting free chunks and coalescing adjacent memory blocks                |
+---------------------------------------|---------------------------------------+
                                        v (Only when allocator runs out of pages)
+-------------------------------------------------------------------------------+
| OS Kernel System Calls (`brk`, `sbrk`, `mmap`, `munmap`, `madvise`)           |
+-------------------------------------------------------------------------------+
```

- **`ptmalloc` (Pthread Malloc)**: The default memory allocator built into the GNU C Standard Library (`glibc`), standard across Linux systems.
- **Allocator Bins**: To find a free memory block without scanning millions of bytes, `ptmalloc` groups free blocks by size into linked-list buckets called **Bins**:
  - *Fast Bins*: Singly-linked lists for tiny allocations ($16\text{B} - 80\text{B}$) designed for maximum speed.
  - *Small Bins*: Circular doubly-linked lists for allocations $< 512\text{B}$.
  - *Large Bins*: Sorted by size for allocations $\ge 512\text{B}$.
- **Chunk Splitting**: If you request 40 bytes, and the allocator only finds a free 100-byte block in a bin, it cuts the 100-byte block into a 40-byte chunk (returned to your code) and a 60-byte remainder chunk (inserted back into a bin for future use).

---

#### 3. Heap Growth Syscalls: `brk` / `sbrk` vs `mmap` / `munmap`
When an allocator runs out of memory in its bins, it requests additional pages from the Linux kernel using two primary system calls:

- **`brk` / `sbrk` (Program Break)**:
  - The traditional heap is a single contiguous region. The boundary line marking the end of the heap is the **"program break"**.
  - `brk(new_addr)` asks the kernel to raise the boundary line to expand heap capacity.
- **`mmap` (Memory Map)**:
  - For large allocations (by default, $\ge 128\text{ KB}$ in `glibc`), putting huge blocks on the contiguous heap causes fragmentation.
  - `mmap` requests the OS to map an independent, isolated block of virtual memory pages anywhere available.
- **`munmap` (Memory Unmap)**:
  - When large memory blocks allocated via `mmap` are freed with `delete` or `free()`, `munmap()` immediately returns those physical pages back to the operating system.

---

#### 4. Deallocation Mechanics: Free-Lists, Coalescing, and `madvise`
- **Free-Lists**: When you call `delete ptr;`, memory is rarely given back to the OS immediately (kernel syscalls are slow). Instead, the allocator tags the chunk as "free" and adds it to an internal Free-List for instant recycling.
- **Coalescing (Merging Free Chunks)**:
  - If Block A (32 bytes) and Block B (64 bytes) sit adjacent to each other and both are freed, the allocator merges them into a single continuous 96-byte chunk.
  - *Why?* Without coalescing, if your program later asks for 80 bytes, neither block would be large enough, causing **External Fragmentation**.
- **`madvise` (Memory Advise Syscall)**:
  - `madvise(addr, length, MADV_DONTNEED)` allows an allocator to advise the Linux kernel: *"I am done using this memory region. You can reclaim the physical RAM frames to give to other processes, but keep my virtual address range valid."* If the process reads from that range again later, the kernel provides a fresh, clean zero-page on demand.

---

#### 5. Latency Determinism: $< 1\text{ ns}$ vs $10\text{ ns} - 100+\text{ ns}$
- **Stack (Deterministic $< 1\text{ ns}$)**: Every stack allocation always executes the exact same single instruction (`sub %rsp, N`). The execution time never varies, never blocks, and never page-faults under normal stack limits.
- **Heap (Non-Deterministic $10\text{ ns} - 100+\text{ ns}$)**:
  - Best case (warm thread cache): $\sim 10\text{ ns}$.
  - Medium case (searching bins, splitting chunks, acquiring arena mutex locks): $\sim 50 - 100\text{ ns}$.
  - Worst case (heap exhaustion triggering `mmap` or `brk` syscalls + OS Page Fault): $\sim 1,000+\text{ ns}$ ($1\mu\text{s} - 10\mu\text{s}$).

---

#### 6. Hardware Cache Locality: L1 CPU Cache vs Main RAM
Modern CPU architectures have a steep latency hierarchy:

```
+-------------------------------------------------------------------------------+
| CPU Registers               | 0-1 Clock Cycles      | < 0.3 ns                |
+-----------------------------+-----------------------+-------------------------+
| L1 Data Cache (32 KB)       | ~4-5 Clock Cycles     | ~1.0 ns                 |
+-----------------------------+-----------------------+-------------------------+
| L2 Cache (512 KB - 1 MB)    | ~14 Clock Cycles      | ~4.0 ns                 |
+-----------------------------+-----------------------+-------------------------+
| L3 Cache (16 MB - 64 MB)    | ~40-60 Clock Cycles   | ~15-20 ns               |
+-----------------------------+-----------------------+-------------------------+
| Main System RAM (DRAM)      | ~200+ Clock Cycles    | ~70-100+ ns             |
+-------------------------------------------------------------------------------+
```

- **Stack Locality**: Because active CPU operations constantly work at the top of the stack, that memory region is almost **permanently hot in the ultra-fast L1 Data Cache** ($1\text{ ns}$ access).
- **Heap Locality**: Dynamic heap objects are scattered across memory. When following pointer chains (e.g. node-based linked lists or trees), if the target address is not in cache, the CPU suffers a **Cache Miss** and stalls for $\sim 70-100\text{ ns}$ waiting for RAM.

---

#### 7. Thread Safety: Thread-Private Stacks vs Per-Thread Heap Arenas
- **Stack**: Every OS thread receives its own dedicated, private stack space. Threads never share stack pointers; allocation on the stack requires **zero locks and zero synchronization**.
- **Heap**: The heap is a single address space shared by all threads in the process.
  - If Thread 1 and Thread 2 both call `malloc()` simultaneously, modifying the same free-list pointers would cause catastrophic memory corruption.
  - Modern allocators solve this by creating **Per-Thread Arenas**: each thread is assigned a distinct heap arena with its own locks and bins, avoiding lock contention between threads.

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
