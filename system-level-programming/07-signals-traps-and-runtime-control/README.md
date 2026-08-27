# Module 07: Signals, Traps, and Runtime Control

An advanced systems engineering reference on Linux signal delivery mechanics, alternate signal stacks (`sigaltstack`), async-signal safety constraints, hardware trap handling (`SIGSEGV`, `SIGBUS`, `SIGFPE`, `SIGILL`), CPU register extraction via `ucontext_t`, and runtime process introspection with `ptrace`.

---

## 1. Linux Signal Delivery Mechanics

Signals are asynchronous notifications delivered by the kernel to a process or specific thread.

```
+-------------------------------------------------------------------------------+
| KERNEL SPACE (Signal Generation)                                              |
|                                                                               |
|  Hardware Exception (e.g. MMU Page Fault on nullptr -> SIGSEGV)               |
|  or Software Syscall (kill(pid, sig) / tgkill())                              |
|  Kernel marks signal bit in task_struct->pending                              |
+---------------------------------------|---------------------------------------+
                                        v (Next Kernel -> User Transition)
+-------------------------------------------------------------------------------+
| KERNEL STACK FRAME SETUP                                                      |
|                                                                               |
|  1. Kernel allocates a `ucontext_t` and `siginfo_t` on the process stack      |
|     (or on `sigaltstack` if SA_ONSTACK was configured).                       |
|  2. Saves current hardware registers (%rip, %rsp, %rax, etc.) into `mcontext`|
|  3. Modifies user return RIP to point to the registered Signal Handler        |
|  4. Sets up `sa_restorer` (trampoline calling rt_sigreturn syscall)           |
+---------------------------------------|---------------------------------------+
                                        v (sysretq to User Space)
+-------------------------------------------------------------------------------+
| USER SPACE SIGNAL HANDLER (Execution)                                         |
|                                                                               |
|  void handler(int sig, siginfo_t* info, void* ucontext)                       |
|                                                                               |
|  * Runs on User Stack or Alternate Stack                                      |
|  * MUST execute only async-signal-safe functions!                             |
|  * On return -> executes trampoline calling `sys_rt_sigreturn`                |
+---------------------------------------|---------------------------------------+
                                        v (rt_sigreturn Syscall)
+-------------------------------------------------------------------------------+
| KERNEL RESTORES ARCHITECTURAL CONTEXT                                         |
|                                                                               |
|  Kernel restores original saved %rip, %rsp, registers and resumes execution   |
+-------------------------------------------------------------------------------+
```

---

## 2. Async-Signal Safety: The Reentrancy Hazard

When a signal arrives, it preempts normal thread execution at an arbitrary instruction boundary.

> [!CAUTION]
> If a thread is in the middle of executing `malloc()`, holding the internal glibc heap arena mutex, and a `SIGSEGV` or `SIGINT` handler also calls `malloc()` or `printf()` / `std::cout`, the process will **deadlock against itself** or corrupt the heap.

### Golden Rules of Async-Signal Safety:
1. **Forbidden in Signal Handlers**: `malloc()`, `free()`, `new`, `delete`, `printf()`, `std::cout`, `pthread_mutex_lock()`, `exit()`, `throw`.
2. **Allowed Primitives**: Direct system calls (`write()`, `read()`, `close()`, `_exit()`), pure memory manipulation on local stack arrays, and `volatile sig_atomic_t` / `std::atomic` stores.

---

## 3. Alternate Signal Stack (`sigaltstack`)

When a thread encounters a **Stack Overflow** (`SIGSEGV` on stack guard page):
- Attempting to invoke a signal handler on the current stack fails immediately (because `%rsp` is already past the stack boundary), causing the kernel to forcefully kill the process.
- **`sigaltstack` Solution**: Allocates a separate dedicated stack buffer in heap/mmap so signal handlers execute safely even when the main thread stack is entirely exhausted.

```cpp
stack_t alt_stack{};
alt_stack.ss_sp = malloc(SIGSTKSZ * 4);
alt_stack.ss_size = SIGSTKSZ * 4;
alt_stack.ss_flags = 0;
sigaltstack(&alt_stack, nullptr);

struct sigaction sa{};
sa.sa_sigaction = &crash_handler;
sa.sa_flags = SA_SIGINFO | SA_ONSTACK; // Use alt stack
sigaction(SIGSEGV, &sa, nullptr);
```

---

## 4. Hardware Traps & CPU Register Extraction

The third argument passed to an `SA_SIGINFO` signal handler is a pointer to `ucontext_t`. On Linux x86_64 (`<sys/ucontext.h>`), `uc_mcontext.gregs` holds the exact CPU register state at the moment of the crash:

| Register Index | Meaning |
|---|---|
| `REG_RIP` | Instruction Pointer (Exact crashing instruction) |
| `REG_RSP` | Stack Pointer |
| `REG_RBP` | Base / Frame Pointer |
| `REG_RAX`, `REG_RBX`, `REG_RCX`, `REG_RDX` | General Purpose Registers |
| `REG_RSI`, `REG_RDI`, `REG_R8`..`REG_R15` | ABI Argument and Callee Registers |
| `REG_EFL` | RFLAGS Register |

`siginfo_t->si_addr` provides the exact faulting memory address (e.g. `0x0` for null pointer dereferences).

---

## 5. Process Control with `ptrace`

The `ptrace(request, pid, addr, data)` system call enables one process (tracer) to inspect and manipulate the execution of another (tracee):

1. **`PTRACE_ATTACH` / `PTRACE_TRACEME`**: Attaches to target process.
2. **`PTRACE_SYSCALL`**: Pauses tracee on every entry and exit from a system call (`strace` engine).
3. **`PTRACE_GETREGS` / `PTRACE_SETREGS`**: Inspects and modifies hardware CPU registers.
4. **Software Breakpoints (`0xCC` / `int 3`)**: A debugger replaces the target instruction byte with `0xCC`. When CPU hits `0xCC`, it triggers a `SIGTRAP` interceptable by `ptrace`. The debugger restores the original byte and single-steps (`PTRACE_SINGLESTEP`).

---

## 💻 Projects in this Module

1. **`projects/crash_debugger_probe/`**:
   - Production-grade crash reporting probe.
   - Automatically registers `sigaltstack`, traps `SIGSEGV`/`SIGFPE`/`SIGILL`, extracts CPU registers from `ucontext_t`, and emits a structured JSON crash dump using 100% async-signal-safe primitives.
