# Module 09: Production Capstone Systems

This module contains three full-scale, production-grade systems architectures synthesizing the entire low-latency and systems programming curriculum:

---

## 🏛️ The Three Capstone Architectures

```
+---------------------------------------------------------------------------------------------------+
| 09-capstone-systems/                                                                              |
|                                                                                                   |
|  [01-shm-orderbook-feed]              [02-iouring-storage-engine]      [03-cooperative-fiber-scheduler]|
|                                                                                                   |
|  * L2 Limit Order Book                * Direct I/O WAL Engine          * M:N Green Thread Runtime |
|  * POSIX Shared Memory                * io_uring Batching              * Work-Stealing Scheduler  |
|  * Wait-Free SPSC Feed                * 4KB Block Alignment            * Fiber Mutex & Channels   |
|  * Sub-Microsecond Tick-to-Trade      * Crash Recovery Loader          * Zero-Allocs Hotpath      |
+---------------------------------------------------------------------------------------------------+
```

### 1. [Capstone 01: Ultra-Low Latency L2 Shared-Memory Orderbook](file:///mnt/c/Users/shrey/OneDrive/Desktop/Code/CPP/cpp-interview/system-level-programming/09-capstone-systems/01-shm-orderbook-feed)
A dual-process HFT architecture consisting of an Exchange Market Data Simulator and an Algorithmic Trading Engine communicating via POSIX shared memory with zero dynamic allocations on the hotpath.

### 2. [Capstone 02: High-Throughput `io_uring` WAL & Storage Engine](file:///mnt/c/Users/shrey/OneDrive/Desktop/Code/CPP/cpp-interview/system-level-programming/09-capstone-systems/02-iouring-storage-engine)
A database storage engine featuring an Append-Only Write-Ahead Log (WAL) with 4KB sector alignment, asynchronous batch submission via Linux `io_uring`, in-memory indexing, and automatic crash recovery.

### 3. [Capstone 03: High-Performance User-Space Cooperative Fiber Scheduler](file:///mnt/c/Users/shrey/OneDrive/Desktop/Code/CPP/cpp-interview/system-level-programming/09-capstone-systems/03-cooperative-fiber-scheduler)
An M:N cooperative green-threading runtime featuring user-space stack switching, lock-free work-stealing across worker threads, and fiber-aware synchronization primitives (`FiberMutex`, `FiberChannel`).
