# Module 02: Virtual Memory Subsystem & Custom Allocators

An advanced engineering guide to modern virtual memory architecture, hardware Translation Lookaside Buffers (TLB), page table translation hierarchies, page fault mechanics, and bare-metal custom memory allocators (Buddy & Slab algorithms) in C++20.

---

## 1. Multi-Level Page Tables & The Memory Management Unit (MMU)

On modern 64-bit architectures, the CPU never accesses physical DRAM directly. The Memory Management Unit (MMU) translates virtual addresses to physical addresses using a multi-level tree structure stored in DRAM and cached in hardware.

### 4-Level Page Table Hierarchy (x86_64, 48-bit Virtual Address Space)

```
64-bit Virtual Address:
+-------------------+---------+---------+---------+---------+---------------+
| Sign Extension    | Level 4 | Level 3 | Level 2 | Level 1 | Offset        |
| (Bits 63..48)     | PGD     | PUD     | PMD     | PTE     | (Bits 11..0)  |
| 16 bits           | 9 bits  | 9 bits  | 9 bits  | 9 bits  | 12 bits (4KB) |
+-------------------+---------+---------+---------+---------+---------------+
                         |         |         |         |            |
                         v         v         v         v            |
                       +-----+   +-----+   +-----+   +-----+        |
      CR3 Register --->| PGD |-->| PUD |-->| PMD |-->| PTE |---> Physical Page Frame
                       +-----+   +-----+   +-----+   +-----+        |
                                                                    v
                                                     [Physical Address = Frame + Offset]
```

- **CR3 Register**: Stores the physical base address of the Page Global Directory (PGD) for the currently running thread.
- **Each Table Level**: 512 entries (each entry is 8 bytes = 4096 bytes / 1 page). 9 bits index into each level ($2^9 = 512$).
- **Translation Cost**: A full translation walk without a TLB hit requires **4 sequential DRAM accesses** (~200–300 CPU cycles).

---

## 2. Translation Lookaside Buffer (TLB) & Shootdown Costs

The TLB is an on-chip associative cache that stores recently resolved `Virtual Page Number -> Physical Frame Number` mappings.

| Level | Latency | Typical Capacity |
|---|---|---|
| **L1 dTLB** (Data TLB) | ~1 cycle | 64 entries (4KB pages) |
| **L2 sTLB** (Shared TLB) | ~7–12 cycles | 1536 entries |
| **Page Table Walk (DRAM)** | ~50–100 ns (150–300 cycles) | Uncached |

### TLB Shootdowns:
When a thread on Core $A$ unmaps (`munmap`) or modifies permissions (`mprotect`) on a shared memory region, the kernel must invalidate stale TLB entries across all other cores running threads of that process:
1. Core $A$ issues an Inter-Processor Interrupt (IPI) to all sibling cores.
2. Cores pause execution, execute the `invlpg` instruction to flush the TLB entry, and acknowledge the IPI.
3. **High Latency Impact**: TLB shootdowns in multi-threaded low-latency systems introduce severe microsecond-scale tail-latency spikes.

### HugePages:
- **Standard Page (4KB)**: 1 TLB entry covers 4 KB.
- **HugePage (2MB)**: 1 TLB entry covers 2 MB ($512\times$ coverage). Eliminates Level 1 (PTE) table walks.
- **Gigantic Page (1GB)**: 1 TLB entry covers 1 GB ($262,144\times$ coverage). Eliminates Level 1 & 2 table walks.

---

## 3. Page Fault Mechanics: Minor vs Major

When a virtual address is accessed whose PTE `Present` bit is 0, the MMU triggers an architectural Page Fault exception (CPU Vector 14), saving the faulting address in `%cr2`.

```
                    CPU Memory Access (Virtual Address)
                                  |
                                  v
                       Is PTE Present == 1?
                             /         \
                       (YES)/           \(NO)
                           v             v
                     Access Memory   Page Fault Exception (Vector 14)
                                         |
                                         v
                             Is VMA mapping valid?
                                  /         \
                            (NO) /           \(YES)
                                v             v
                         SIGSEGV (Crash)  Fault Type Check
                                          /          \
                         (File / Swap backed)      (Anonymous Zero Page)
                                  v                          v
                           MAJOR Page Fault           MINOR Page Fault
                           - Suspends Thread          - Allocates physical page
                           - Synchronous Disk I/O     - Zeroes page buffer
                           - High Latency (ms)        - Updates PTE Present = 1
                                                      - Fast (~1-2 microseconds)
```

1. **Minor (Soft) Page Fault**:
   - The page is not present in the hardware page table, but physical memory is readily available.
   - Example: **Demand Paging** upon the first write to an anonymous `mmap` allocation, or **Copy-on-Write (COW)** on `fork()`.
2. **Major (Hard) Page Fault**:
   - The page data must be read synchronously from disk or swap storage.
   - Suspends the thread for milliseconds while waiting on block storage I/O.

---

## 4. Linux Virtual Memory Control Primitives

- `mmap(addr, len, prot, flags, fd, offset)`: Creates a new virtual memory area (VMA) in the process address space.
- `munmap(addr, len)`: Removes the VMA mapping and invalidates associated TLB entries.
- `mprotect(addr, len, prot)`: Changes access protections (`PROT_READ`, `PROT_WRITE`, `PROT_EXEC`).
- `madvise(addr, len, advice)`:
  - `MADV_DONTNEED`: Releases physical pages back to the kernel immediately while retaining the virtual address space.
  - `MADV_HUGEPAGE`: Advises Transparent Hugepage (THP) daemon to back the range with 2MB huge pages.
- `mlock(addr, len)`: Locks memory range into physical RAM, disabling swapping and eliminating major page faults on hot paths.

---

## 5. Custom Memory Allocator Architectures

General-purpose allocators (`ptmalloc`, `tcmalloc`, `jemalloc`) incur metadata overhead, cache line hopping, and lock contention. Low-latency systems deploy tailored allocation strategies:

### A. Binary Buddy Allocator
- Manages raw memory blocks in discrete powers of two ($2^{\text{order}}$ pages).
- **Split**: If a requested order is unavailable, a larger block is recursively split in half into "buddies".
- **Merge**: When a block is freed, its buddy address (`addr ^ block_size`) is checked. If free, they are recursively merged to prevent external fragmentation.

### B. Fixed-Size Slab Allocator
- Allocates fixed-size chunks from pre-allocated memory slabs.
- **Intrusive Free-List**: Unallocated chunks store pointers to the next free chunk *inside their own memory body*, guaranteeing $O(1)$ allocation and zero external bookkeeping overhead.

---

## 💻 Lab Exercises & Projects in this Module

1. **`exercises/page_fault_tracker.hpp`**:
   - Programmatically measures soft vs hard page faults using Linux `getrusage` and `/proc/self/smaps`.
2. **`projects/slab_and_buddy_allocator/`**:
   - Production-grade **Buddy Allocator** and cache-aligned **Slab Allocator** written from scratch using raw `mmap` backing with zero runtime heap calls.
   - Includes full unit test suite and nanosecond benchmark vs `malloc`/`free`.
