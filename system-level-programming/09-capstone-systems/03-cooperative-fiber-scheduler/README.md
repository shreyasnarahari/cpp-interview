# Capstone 03: High-Performance User-Space Cooperative Fiber Scheduler

A production-grade, M:N cooperative green-threading runtime featuring user-space stack switching, lock-free work distribution, and fiber-aware synchronization primitives (`FiberMutex`, `FiberChannel`).

---

## 🏛️ System Architecture

```
+-------------------------------------------------------------------------------+
| APPLICATION CODE (Fibers / Green-Threads)                                     |
|                                                                               |
|  Fiber 1 (Network)        Fiber 2 (Calculation)      Fiber 3 (Disk I/O)       |
|  Calls yield()            Calls mutex.lock()         Calls chan.send()        |
+-------------------|-------------------|-------------------|-------------------+
                    v                   v                   v
+-------------------------------------------------------------------------------+
| COOPERATIVE FIBER RUNTIME (User Space Context Switcher)                       |
|                                                                               |
|  * Dedicated 64KB Stacks per Fiber                                            |
|  * Fast Stack Pointer & Register Swapping (swapcontext / asm) (~15-25 ns)     |
|  * Zero Kernel Transitions on Context Switch                                  |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| WORK-STEALING FIBER SCHEDULER                                                 |
|                                                                               |
|  [Worker Thread 0: Run Queue]      [Worker Thread 1: Run Queue]               |
|  * Executes active fibers          * Steals tasks when queue is empty         |
+-------------------------------------------------------------------------------+
```

---

## 🔬 Fiber Context Switching vs OS Thread Context Switching

| Metric | OS Thread (Kernel Switch) | User-Space Fiber (Cooperative) |
|---|---|---|
| **Mechanism** | Syscall / Preemption interrupt, Ring 3 -> Ring 0, saves full FPU/SSE/AVX state | Stack Pointer (`%rsp`) & Callee-saved register swap in user space |
| **Cost** | $\sim 1,000\text{--}2,000\text{ ns}$ ($1\text{--}2\text{ us}$) | $\sim 15\text{--}30\text{ ns}$ |
| **Stack Memory** | $2\text{ MB} - 8\text{ MB}$ per thread | $16\text{ KB} - 64\text{ KB}$ per fiber |
| **Concurrency** | $1,000 - 10,000$ max threads before OS thrashing | $100,000 - 1,000,000+$ simultaneous fibers |

---

## 🔬 Fiber-Aware Synchronization

When a fiber calls `FiberMutex::lock()` on a contended mutex:
- It **does not block the underlying OS worker thread**!
- Instead, it marks itself `SUSPENDED`, enqueues onto the mutex's waiter list, and invokes `Scheduler::yield()`, allowing the worker thread to immediately execute another ready fiber.
