# Module 08: Kernel Diagnostics, eBPF, and Profiling

An exhaustive guide to Linux Performance Monitoring Units (PMU), the `perf` subsystem, the extended Berkeley Packet Filter (eBPF) runtime, dynamic user-space tracing (Uprobes/Uretprobes), and the internal mechanics of compiler sanitizers (ASan & TSan).

---

## 1. Hardware PMUs & The Linux `perf` Architecture

Modern CPUs contain dedicated Hardware Performance Monitoring Units (PMUs) capable of counting microarchitectural events with zero instruction-level instrumentation.

### Key Hardware PMU Counters:
- **`instructions` vs `cycles`**: Instructions per Cycle ($\text{IPC} = \frac{\text{Instructions}}{\text{Cycles}}$). Standard target: $\ge 2.0\text{ IPC}$.
- **`cache-misses` & `cache-references`**: L3 cache miss rate.
- **`branch-misses` & `branch-instructions`**: Branch misprediction rate (target $<1\%$).
- **`stalled-cycles-backend`**: Cycles CPU is stalled waiting for memory DRAM accesses.

```
+-------------------------------------------------------------------------------+
| CPU Core Pipeline                                                             |
|                                                                               |
|  [Hardware PMU Counter 0: Instructions] [Hardware PMU Counter 1: LLC Misses]  |
|                                |                                              |
|                                v (PMU Overflow Interrupt)                     |
+-------------------------------------------------------------------------------+
| Linux perf Subsystem (Kernel)                                                 |
|                                                                               |
|  Sample Ring Buffer -> perf_event_open() -> User Space (`perf record`)        |
+-------------------------------------------------------------------------------+
```

---

## 2. Extended Berkeley Packet Filter (eBPF) Architecture

eBPF is an in-kernel RISC-V/x86-like virtual machine with 11 64-bit registers (`r0`–`r10`) that allows running sandboxed bytecode inside the Linux kernel at native speed via JIT compilation.

```
+-------------------------------------------------------------------------------+
| USER SPACE                                                                    |
|                                                                               |
|  C / Rust eBPF Source -> Clang / LLVM (`-target bpf`) -> eBPF Bytecode        |
|  Loads bytecode via `bpf(BPF_PROG_LOAD, ...)`                                 |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| KERNEL SPACE                                                                  |
|                                                                               |
|  1. In-Kernel Verifier (Checks DAG, loop bounds, memory access bounds)        |
|  2. JIT Compiler (Translates BPF bytecode directly to native x86_64 machine code)|
|  3. Attaches to Trace Hook:                                                   |
|     - kprobe / kretprobe (Kernel function entry / exit)                       |
|     - uprobe / uretprobe (User-space application function entry / exit)       |
|     - tracepoints (Static kernel trace events)                                |
|  4. Aggregates data into BPF Maps (Hash Maps, Ring Buffers)                   |
+---------------------------------------|---------------------------------------+
                                        v
| User Space Reads Metrics from BPF Map via `bpf(BPF_MAP_LOOKUP_ELEM)`          |
+-------------------------------------------------------------------------------+
```

### Dynamic User-Space Tracing: Uprobes & Uretprobes
- **Uprobe**: Kernel dynamically replaces the first instruction byte of a target binary's function in user space with a breakpoint (`0xCC` / `int 3`). When hit, control transfers to the eBPF program, then resumes the original instruction.
- **Uretprobe**: Traces function exit, allowing computation of function execution duration:
  $$\Delta t = t_{\text{exit}} - t_{\text{entry}}$$
- **Zero Recompilation**: Hooks production C++ binaries in real-time without recompilation or restarting processes!

---

## 3. Sanitizer Internals: ASan & TSan Mechanics

### A. AddressSanitizer (ASan) Shadow Memory
ASan maps 1 byte of **Shadow Memory** for every 8 bytes of application memory:
$$\text{ShadowAddress} = (\text{AppAddress} \gg 3) + \text{Offset}$$

```
Application Memory (8 Bytes)          Shadow Byte (1 Byte)
+---+---+---+---+---+---+---+---+     +----+
| a | b | c | d | e | f | g | h | --> | 00 |  (All 8 bytes addressable)
+---+---+---+---+---+---+---+---+     +----+
| a | b | c | d | X | X | X | X | --> | 04 |  (Only first 4 bytes addressable)
+---+---+---+---+---+---+---+---+     +----+
| RED ZONE (Guards around arrays)| --> | FA |  (Heap Left Redzone - Access = Crash)
+---+---+---+---+---+---+---+---+     +----+
| FREED HEAP MEMORY             | --> | FD |  (Use-After-Free - Access = Crash)
+---+---+---+---+---+---+---+---+     +----+
```

### B. ThreadSanitizer (TSan) Vector Clocks & State Machines
TSan tracks a shadow state for each 8-byte application memory location, storing the thread ID and logical timestamp of the last reader/writer. If two threads access the same address with at least one writer and without an established **Happens-Before relationship** (via atomic release-acquire or mutex), TSan flags a Data Race.

---

## 💻 Lab Exercises in this Module

1. **`exercises/ebpf_uprobe_latency/`**:
   - `target_app.cpp`: Low-latency C++ workload containing hotpath order-matching functions.
   - `trace_uprobe.py`: Python/BCC eBPF uprobe latency tracer capturing nanosecond function distributions non-invasively.
