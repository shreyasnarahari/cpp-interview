# Module 01: OS ABI, Syscall Internals, and Binary Anatomy

A deep-dive technical reference on the System V AMD64 Application Binary Interface (ABI), user-to-kernel mode transitions, CPU privilege level switches, raw system call mechanics, and ELF64 binary internals.

---

## 1. System V AMD64 ABI Calling Convention

The Application Binary Interface (ABI) defines the low-level machine code interface between independent translation units, libraries, and the operating system.

### Register Allocation & Passing Protocol (x86_64)

In the System V AMD64 ABI (standard across Linux, BSD, macOS on x86_64):

| Purpose | Registers in Sequence | Notes |
|---|---|---|
| **Function Arguments (Integer/Pointer)** | `%rdi`, `%rsi`, `%rdx`, `%rcx`, `%r8`, `%r9` | Arguments 1 through 6 passed in hardware registers |
| **Additional Arguments (7+)** | Stack (`(%rsp)`) | Pushed in reverse order (right-to-left) |
| **Return Value** | `%rax` (Primary), `%rdx` (Secondary 128-bit) | Integers, pointers, small structs |
| **Floating-Point Arguments** | `%xmm0` – `%xmm7` | Passed in SSE registers |
| **Callee-Saved Registers** | `%rbx`, `%rsp`, `%rbp`, `%r12`, `%r13`, `%r14`, `%r15` | Must be preserved across function calls |
| **Caller-Saved Registers** | `%rax`, `%rcx`, `%rdx`, `%rsi`, `%rdi`, `%r8`–`%r11` | Volatile across function calls |

### Stack Alignment & Red Zone

- **16-Byte Stack Alignment**: Before executing a `call` instruction, the stack pointer `%rsp` must be 16-byte aligned (`(%rsp + 8) % 16 == 0` upon function entry due to the 8-byte return address pushed by `call`).
- **The 128-Byte Red Zone**: A 128-byte region *below* the current `%rsp` (`%rsp - 1` to `%rsp - 128`) reserved for leaf functions. The compiler can store local variables here without adjusting `%rsp`, guaranteed not to be clobbered by signal handlers or interrupts on Linux x86_64.

```
       High Memory Address
   +--------------------------+
   |   Function Argument 7    |  (%rsp + 16)
   +--------------------------+
   |   Return Address (RIP)   |  (%rsp + 8)  <- Pushed by CALL
   +--------------------------+
   |    Saved %rbp (Frame)    |  (%rsp)      <- Current Stack Top
   +--------------------------+
   |     Local Variables      |
   +--------------------------+
   |   128-Byte RED ZONE      |  (%rsp - 128 to %rsp - 1)
   +--------------------------+  <- Safe for leaf functions
       Low Memory Address
```

---

## 2. User Space to Kernel Space Transition (The `syscall` Instruction)

In early x86 architectures, software interrupts (`int 0x80`) were used to invoke the kernel. Modern x86_64 uses the dedicated `syscall` instruction.

```
+--------------------------------------------------------------------------------+
| USER SPACE (Ring 3)                                                            |
|                                                                                |
|  1. Load Syscall Number into %rax (e.g., 1 for sys_write)                      |
|  2. Load Arguments into: %rdi, %rsi, %rdx, %r10, %r8, %r9 (Notice %r10 != %rcx)|
|  3. Execute `syscall` Instruction                                              |
+---------------------------------------+----------------------------------------+
                                        |  Hardware Privilege Switch (Ring 3 -> Ring 0)
                                        |  Saves User RIP -> %rcx, User RFLAGS -> %r11
                                        |  Loads Kernel RIP from MSR IA32_LSTAR
                                        v
+--------------------------------------------------------------------------------+
| KERNEL SPACE (Ring 0)                                                          |
|                                                                                |
|  4. Hardware sets CPL = 0 (Ring 0)                                             |
|  5. `swapgs` switches to Kernel GS base (Kernel Per-CPU data structure)        |
|  6. Switches `%rsp` to Kernel Task Stack (`task_struct->thread.sp0`)           |
|  7. Pushes pt_regs (saving user-space register context)                        |
|  8. Dispatches to sys_call_table[%rax]                                         |
|  9. Executes kernel implementation (e.g. vfs_write)                            |
| 10. Stores return code in %rax (negative value represents -errno)              |
| 11. Restores user registers from pt_regs                                       |
| 12. Executes `sysretq` instruction                                             |
+---------------------------------------+----------------------------------------+
                                        |  Hardware Privilege Switch (Ring 0 -> Ring 3)
                                        |  Restores User RIP from %rcx, RFLAGS from %r11
                                        v
+--------------------------------------------------------------------------------+
| USER SPACE (Ring 3)                                                            |
|                                                                                |
|  13. User process resumes immediately after `syscall` instruction              |
|  14. Reads return value / status code from %rax                                |
+--------------------------------------------------------------------------------+
```

### Syscall Calling Convention vs C ABI Calling Convention

> [!IMPORTANT]
> The Linux kernel syscall convention differs slightly from standard C ABI:
> - Argument 4 is passed in **`%r10`**, NOT `%rcx` (because `syscall` hardware instruction destroys `%rcx` to store the return `%rip`).
> - The syscall number is placed in **`%rax`**.
> - Kernel return value is returned in **`%rax`**. A return value in the range `[-4095, -1]` indicates an error (`-errno`).

---

## 3. Binary Anatomy: The ELF64 Executable Format

An Executable and Linkable Format (ELF64) file contains both **Segments** (used by the OS dynamic loader `ld.so` at runtime) and **Sections** (used by the compiler and static linker `ld` at build time).

```
+--------------------------------------------------------------------+
|                         ELF64 Header (64 B)                        |
|  - Magic: 0x7F 'E' 'L' 'F' (EI_MAG0..3)                            |
|  - Machine Type (EM_X86_64), Entry Point Address (e_entry)         |
|  - Program Header Table Offset (e_phoff)                           |
|  - Section Header Table Offset (e_shoff)                           |
+--------------------------------------------------------------------+
|               Program Header Table (Segments / Load Map)           |
|  - PT_LOAD: Executable Code (R-X)  -> Mapped to memory via mmap    |
|  - PT_LOAD: Read-Only Data  (R--)  -> Const data, strings          |
|  - PT_LOAD: Data / BSS      (RW-)  -> Initialized & Zeroed vars    |
|  - PT_DYNAMIC: Dynamic Linker metadata (.dynamic)                  |
|  - PT_INTERP: Path to dynamic linker (e.g., /lib64/ld-linux-x86-64)|
|  - PT_GNU_STACK: Stack execution permissions (NX bit enforcement)  |
+--------------------------------------------------------------------+
|                         Sections (Linking View)                    |
|  - .text: Compiled machine instructions (R-X)                      |
|  - .rodata: String literals, constexpr tables, const data (R--)   |
|  - .data: Initialized global and static variables (RW-)            |
|  - .bss: Uninitialized global/static variables (Zeroed, 0 bytes)   |
|  - .symtab / .strtab: Symbol table and string table (Debug/link)   |
|  - .dynsym / .dynstr: Dynamic symbol & string tables               |
|  - .plt / .got: Procedure Linkage Table & Global Offset Table      |
+--------------------------------------------------------------------+
|                        Section Header Table                        |
|  - Describes names, types, flags, memory offsets of all sections   |
+--------------------------------------------------------------------+
```

### Dynamic Linking & PLT / GOT Mechanics

When calling an external library function (e.g. `puts` from `libc.so`):
1. The code calls `puts@plt` (an entry in the Procedure Linkage Table).
2. The PLT jumps indirectly through the Global Offset Table (`GOT[puts]`).
3. **First Call (Lazy Binding)**: The GOT initially points back to the dynamic resolver routine in the PLT. The dynamic linker resolves the real address of `puts` in memory and updates `GOT[puts]`.
4. **Subsequent Calls**: `puts@plt` jumps directly through `GOT[puts]` to the resolved library function without dynamic linker intervention.

---

## 💻 Lab Exercises & Projects in this Module

1. **`exercises/raw_syscalls.hpp`**:
   - Direct x86_64 inline assembly syscall implementations without `libc`.
   - Standalone `-nostdlib` binary running directly on Linux without standard runtime libraries.
2. **`tests/test_raw_syscalls.cpp`**:
   - Unit test suite verifying raw file descriptors, string writes, read buffers, and exit codes.
3. **`projects/lightweight_elf_inspector/`**:
   - Production-grade C++20 tool for parsing raw ELF64 binaries, decoding headers, mapping memory permissions (`R`, `W`, `X`), and locating execution entry points.
