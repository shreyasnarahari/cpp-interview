# Module 05: Advanced I/O and Event Engines

An in-depth technical engineering reference on the Linux I/O subsystem, socket buffer mechanics, TCP flow control, non-blocking asynchronous event loops, Edge-Triggered `epoll`, the Linux `io_uring` architecture, and Zero-Copy data transmission.

---

## 1. Linux File Descriptor Tables & Socket Buffers

Every Linux process maintains a File Descriptor (FD) table mapping integer indices to open file/socket objects in the kernel Virtual File System (VFS).

```
+--------------------------------------------------------------------------------+
| USER SPACE                                                                     |
|                                                                                |
|  int fd = 4; ---> write(fd, buf, len)           read(fd, buf, len)             |
+--------------------------|-------------------------------^---------------------+
                           v                               |
+--------------------------------------------------------------------------------+
| KERNEL SPACE (VFS & Network Stack)                                             |
|                                                                                |
|  Process FD Table -> struct file -> struct socket -> struct sock (SKB Queues) |
|                                                                                |
|  [SO_SNDBUF (Send Queue)]                           [SO_RCVBUF (Receive Queue)]|
|  +-----+-----+-----+-----+                          +-----+-----+-----+-----+  |
|  | SKB | SKB | SKB | SKB |                          | SKB | SKB | SKB | SKB |  |
|  +-----+-----+-----+-----+                          +-----+-----+-----+-----+  |
|             |                                                    ^             |
|             v (TCP Segment Flow Control)                         |             |
|  [NIC Driver Ring Buffer (TX)]                      [NIC Driver Ring (RX)]     |
+--------------------------------------------------------------------------------+
```

### TCP Backpressure & Non-Blocking Sockets (`O_NONBLOCK`)
- **`SO_SNDBUF` Full**: When the remote peer's TCP window is saturated or network bandwidth is throttled, the local `SO_SNDBUF` fills up. On a blocking socket, `write()` halts thread execution. On a non-blocking socket (`O_NONBLOCK`), `write()` immediately returns `-1` with `errno == EAGAIN` or `EWOULDBLOCK`.
- **`SO_RCVBUF` Empty**: On non-blocking sockets, `read()` returns `-1` with `EAGAIN` when no new packets have arrived.

---

## 2. I/O Multiplexing: `select`/`poll` vs `epoll`

| Mechanism | Kernel Complexity | Event Notification | Scalability |
|---|---|---|---|
| **`select()` / `poll()`** | $O(N)$ linear scan over all watched FDs on every wakeup | Linear list pass | Inefficient for $>1,000$ active connections |
| **`epoll`** | $O(1)$ red-black tree lookup + ready list callback | Wakeup returns *only* active ready FDs | Scales effortlessly to $100,000+$ connections |

### Edge-Triggered (`EPOLLET`) vs Level-Triggered (`EPOLLIN`)
- **Level-Triggered (Default)**: `epoll_wait()` notifies repeatedly as long as bytes remain in the socket receive buffer. Forgiving of partial reads, but generates redundant system call wakeups.
- **Edge-Triggered (`EPOLLET`)**: `epoll_wait()` notifies **only on state transitions** (e.g. when new bytes arrive on the wire).
  - **The Golden Rule of ET**: The application **must** loop `read()` / `write()` until receiving `EAGAIN` / `EWOULDBLOCK`. If you stop reading before `EAGAIN`, the remaining bytes will sit in the buffer forever without triggering another wakeup!

---

## 3. Linux `io_uring`: Asynchronous Kernel Ring Buffers

Introduced in Linux 5.1+, `io_uring` replaces traditional system calls with a pair of lock-free circular ring buffers mapped into both user-space and kernel memory via `mmap`.

```
+-------------------------------------------------------------------------+
| USER SPACE                                                              |
|                                                                         |
|  1. Writes Submission Queue Entry (SQE) into SQ Ring                   |
|  2. Updates SQ Tail -> sq_ring->tail.store(..., memory_order_release)   |
|  3. Invokes io_uring_enter() (or 0 syscalls in IORING_SETUP_SQPOLL mode)|
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
| KERNEL SPACE                                                            |
|                                                                         |
|  4. Kernel consumes SQEs, dispatches async I/O to storage / network     |
|  5. On I/O completion, kernel writes Completion Queue Entry (CQE) to CQ |
|  6. Updates CQ Tail -> cq_ring->tail.store(..., memory_order_release)   |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
| USER SPACE                                                              |
|                                                                         |
|  7. Reads completed CQEs directly from CQ Ring                          |
|  8. Advances CQ Head -> cq_ring->head.store(..., memory_order_release)   |
+-------------------------------------------------------------------------+
```

### Key Advantages of `io_uring`:
1. **Zero System Call Overhead**: Batched submission (1 syscall submits 1,000 requests) or pure zero-syscall I/O via `IORING_SETUP_SQPOLL` (kernel background polling thread).
2. **Unified API**: Asynchronous file I/O, socket I/O (`accept`, `read`, `write`, `connect`), and timers through one interface.
3. **Fixed Buffers & Fixed Files**: Pre-registers memory buffers with the kernel (`io_uring_register`), eliminating page mapping and pinning overhead on hotpaths.

---

## 4. Zero-Copy Data Transmission Primitives

| System Call | Mechanism | Best Use Case |
|---|---|---|
| **`sendfile(out_fd, in_fd, offset, count)`** | Transfers pages directly from file page cache into the target socket without copying into user space. | Static file streaming (HTTP web servers) |
| **`splice(fd_in, off_in, fd_out, off_out, len, flags)`** | Moves data between a file/socket and a pipe buffer via kernel page reference transfer. | Zero-copy proxying & packet forwarding |
| **`vmsplice()`** | Maps user memory pages directly into a pipe without copying. | User-space to kernel streaming |

---

## 💻 Lab Exercises & Projects in this Module

1. **`exercises/epoll_server.hpp` & `exercises/epoll_edge_triggered.cpp`**:
   - Production-grade non-blocking TCP echo server using edge-triggered `epoll` (`EPOLLET`), full buffer draining, and error handling.
2. **`tests/test_epoll_server.cpp`**:
   - Multi-threaded concurrent TCP client test verifying reliable echo roundtrips.
3. **`projects/iouring_echo_reactor/`**:
   - High-throughput asynchronous TCP reactor built on raw Linux `io_uring` ring architecture.
   - Includes high-concurrency client load benchmark (`bench_client.cpp`).
