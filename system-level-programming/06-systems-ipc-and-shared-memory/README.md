# Module 06: Systems IPC and Shared Memory

An exhaustive, low-latency engineering guide to Inter-Process Communication (IPC), POSIX shared memory (`/dev/shm`), cross-process atomic synchronization, the Linux `futex` (Fast Userspace Mutex) system call, and Unix Domain Socket file descriptor passing (`SCM_RIGHTS`).

---

## 1. POSIX Shared Memory vs System V IPC

Shared memory is the highest-throughput IPC mechanism available on Linux because data is read and written directly to physical RAM without kernel copies, context switches, or socket buffer encapsulation.

| Feature | POSIX Shared Memory (`shm_open`) | System V IPC (`shmget` / `shmat`) |
|---|---|---|
| **Naming** | Filesystem path in `/dev/shm/name` | Numeric `key_t` (e.g. `ftok`) |
| **Interface** | Standard file descriptor (`int fd`), `mmap()`, `close()` | Obsolete `shmid`, `shmat()`, `shmctl()` |
| **Permissions** | Standard POSIX file permissions (`0666`) | `ipc_perm` flags |
| **Inspection** | Inspectable via `ls -la /dev/shm` / standard file tools | Requires `ipcs -m` / `ipcrm` |
| **Recommended** | **Yes (Modern Standard)** | No (Legacy) |

```
+------------------------------------+------------------------------------+
| PROCESS A (Producer)               | PROCESS B (Consumer)               |
|                                    |                                    |
| 1. fd = shm_open("/market_feed",..)| 1. fd = shm_open("/market_feed",..)|
| 2. ftruncate(fd, 64 * 1024 * 1024) | 2. [No truncate]                   |
| 3. ptr = mmap(..., MAP_SHARED, fd) | 3. ptr = mmap(..., MAP_SHARED, fd) |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
| PHYSICAL RAM (Backing: tmpfs /dev/shm/market_feed)                      |
|                                                                         |
|  [Header: Atomic Tail (64B)] [Header: Atomic Head (64B)] [Ring Buffer]  |
|                                                                         |
|  * Reads and Writes execute at raw DRAM bus speed (~10-20 GB/sec)       |
|  * 0 System calls during steady-state messaging                         |
+-------------------------------------------------------------------------+
```

---

## 2. Cross-Process Synchronization: The Linux `futex` System Call

Standard `std::mutex` does not work across processes by default. To synchronize separate processes mapping the same shared memory region, Linux provides the `futex` (Fast Userspace Mutex) architecture.

### The Two-Tier Architecture of a Futex Mutex:
1. **Fast-Path (User Space - 0 System Calls)**:
   - When acquiring an uncontended lock, an atomic Compare-And-Swap (`std::atomic<uint32_t>::compare_exchange_strong`) changes state from `UNLOCKED (0)` to `LOCKED (1)`.
   - Latency: **$\sim 10\text{--}15\text{ CPU cycles}$ ($\sim 3\text{ ns}$)**.
2. **Slow-Path (Kernel Space - Sleep on Contention)**:
   - If the lock is already held, state transitions to `CONTENDED (2)`.
   - The thread calls `syscall(SYS_futex, uaddr, FUTEX_WAIT, 2, timeout, ...)` to sleep in the kernel wait queue until woken.
   - Upon release, `syscall(SYS_futex, uaddr, FUTEX_WAKE, 1, ...)` wakes 1 waiting process.

```cpp
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

long futex_wait(uint32_t* uaddr, uint32_t expected_val) {
    return ::syscall(SYS_futex, uaddr, FUTEX_WAIT, expected_val, nullptr, nullptr, 0);
}

long futex_wake(uint32_t* uaddr, int count) {
    return ::syscall(SYS_futex, uaddr, FUTEX_WAKE, count, nullptr, nullptr, 0);
}
```

---

## 3. Unix Domain Sockets (UDS) & File Descriptor Passing (`SCM_RIGHTS`)

Unix Domain Sockets (`AF_UNIX`) allow passing open kernel file descriptors between completely unrelated processes using ancillary control messages (`struct cmsghdr` with `SCM_RIGHTS`).

```
Process A (Holds open tun/tap or socket fd=5)
   |
   | sendmsg(uds_sock, &msg) with cmsg.cmsg_type = SCM_RIGHTS (fd=5)
   v
[UNIX DOMAIN SOCKET]
   |
   | recvmsg(uds_sock, &msg) -> Kernel duplicates struct file into Process B
   v
Process B (Receives new valid fd=12 pointing to the identical open socket!)
```

This pattern is fundamental in:
- High-Performance Load Balancers (HAProxy, Envoy): Handoff active TCP sockets without closing connections.
- Sandboxed Worker Runtimes: Parent process opens privileged resources and transfers file descriptors to unprivileged worker sandboxes.

---

## 4. Ultra-Low Latency Lock-Free Shared Memory Ring Buffer

By placing our wait-free SPSC circular queue directly into `/dev/shm`:
- Processes communicate with **sub-500 nanosecond end-to-end latency**.
- Zero dynamic memory allocation on message transmission.
- Cache-line isolation (`alignas(64)`) prevents cross-socket bus contention across NUMA domains.

---

## 💻 Lab Exercises & Projects in this Module

1. **`exercises/futex_mutex.hpp` & `exercises/shm_futex_sync.cpp`**:
   - Production C++20 `FutexMutex` implementing raw user-space atomic fast-path and kernel `FUTEX_WAIT`/`FUTEX_WAKE` fallback.
2. **`tests/test_shm_futex.cpp`**:
   - Multi-process test (`fork()`) validating race-free concurrent increments on a shared memory counter.
3. **`projects/low_latency_shm_ipc_bus/`**:
   - Shared-memory IPC pub-sub message bus with zero-copy binary message framing.
   - Comprehensive multi-process test and sub-500ns latency benchmark.
