# Nokia — C++ Interview Questions & Systems Reference

> **Focus:** Embedded C++, Memory Management, Telecommunications & 5G Stacks, Socket Programming, Linux System Calls, RTOS, Concurrency, and Low-Level Performance

---

## Overview of Nokia C++ Interviews

Nokia technical rounds focus heavily on **systems-level programming, network protocols, memory efficiency, and core C++ fundamentals**:
- **Core C++ & Memory**: Pointers vs references, smart pointer mechanics, custom memory pools, bitwise operations, RAII, and object-oriented design.
- **Networking & System Calls**: Linux socket API (`socket`, `bind`, `listen`, `accept`, `connect`), non-blocking I/O (`epoll`/`select`), network byte order, and packet buffering.
- **Concurrency & RTOS**: Mutex vs semaphores, Inter-Process Communication (IPC via shared memory, message queues, sockets), priority inversion, atomic operations, and data race prevention.
- **Practical Coding & Debugging**: Linked list manipulations, bitwise flag operations, string/buffer parsing, and memory leak analysis (`valgrind`, `ASan`, GDB core dumps).

---

## 52 Interview Questions & Solutions

### Category 1: Memory Management, Pointers & Embedded C++

#### Q1. `malloc`/`free` vs `new`/`delete` vs Placement `new`
- **`malloc`/`free`**: C library functions. Allocates raw uninitialized byte buffers from the heap. Does NOT invoke constructors or destructors. Returns `void*`.
- **`new`/`delete`**: C++ type-safe operators. Allocates memory AND calls object constructors/destructors. Throws `std::bad_alloc` on failure.
- **Placement `new`**: Constructs an object at a pre-allocated raw memory address without heap allocation (`new (address) T(args)`). Destructor MUST be called explicitly (`ptr->~T()`).

```cpp
#include <new>
#include <iostream>

struct Packet {
    int id;
    Packet(int i) : id(i) { std::cout << "Packet Constructed\n"; }
    ~Packet() { std::cout << "Packet Destroyed\n"; }
};

int main() {
    alignas(alignof(Packet)) char buffer[sizeof(Packet)]; // Pre-allocated stack memory
    Packet* p = new (buffer) Packet(101); // Placement new
    p->~Packet(); // Explicit destructor (Do NOT call delete p!)
}
```

---

#### Q2. Program Memory Layout (Text, Data, BSS, Heap, Stack)

```
                       C/C++ Virtual Address Space:
  ┌─────────────────────────────────────────────────────────────┐ High Address
  │ Environment & Command Line Arguments                        │
  ├─────────────────────────────────────────────────────────────┤
  │ Stack Segment (Local variables, function frames) ↓          │
  ├─────────────────────────────────────────────────────────────┤
  │ Shared Libraries & Memory Mapped Regions                    │
  ├─────────────────────────────────────────────────────────────┤
  │ Heap Segment (Dynamic malloc/new allocations) ↑             │
  ├─────────────────────────────────────────────────────────────┤
  │ BSS Segment (Uninitialized static/global variables = 0)     │
  ├─────────────────────────────────────────────────────────────┤
  │ Data Segment (Initialized static/global variables)          │
  ├─────────────────────────────────────────────────────────────┤
  │ Text Segment (Compiled Machine Code / Instructions)         │
  └─────────────────────────────────────────────────────────────┘ Low Address
```

---

#### Q3. Pointer Arithmetic, Array Decay & Pointer vs Reference
- **Array Decay**: Passing an array (`int arr[10]`) into a function decays it to a raw pointer (`int*`). `sizeof(arr)` inside the function yields `sizeof(int*)` (8 bytes), NOT the total array byte size!
- **Pointer vs Reference**:
  - Pointer: Stores a memory address, can be `nullptr`, can rebind to point to another variable.
  - Reference: Immutable alias for an existing object. Cannot be `nullptr`, cannot rebind after initialization.

```cpp
void inspectArray(int arr[10]) { // Decays to int* arr!
    // sizeof(arr) == 8 bytes (pointer size), NOT 40 bytes!
}
```

---

#### Q4. Function Pointers vs `std::function` in C-Callback vs C++ Handlers
- **C Function Pointer (`void (*fp)(int)`)**: Zero-overhead 8-byte pointer to code. Cannot capture state or bind to non-static member functions.
- **`std::function<void(int)>`**: Type-erased wrapper supporting lambdas, stateful closures, and `std::bind`. Incurs small buffer optimization (SBO) or heap allocation + indirect virtual call overhead.

---

#### Q5. Bit Manipulation: Check Power of 2
**Question:** Write a function to check if a positive integer is a power of 2 using bitwise operators.

```cpp
bool isPowerOfTwo(unsigned int n) {
    // Power of 2 has exactly 1 bit set in binary (e.g. 1000 = 8).
    // n & (n - 1) clears the lowest set bit.
    return n > 0 && (n & (n - 1)) == 0;
}
```

---

#### Q6. Bitwise Operations: Set, Clear, Toggle, and Extract Bitfields

```cpp
#include <cstdint>

uint32_t setBit(uint32_t val, int bit_pos) { return val | (1U << bit_pos); }
uint32_t clearBit(uint32_t val, int bit_pos) { return val & ~(1U << bit_pos); }
uint32_t toggleBit(uint32_t val, int bit_pos) { return val ^ (1U << bit_pos); }
bool isBitSet(uint32_t val, int bit_pos) { return (val & (1U << bit_pos)) != 0; }

// Extract 4-bit protocol field at offset 8:
uint32_t getField(uint32_t header) {
    return (header >> 8) & 0x0F;
}
```

---

#### Q7. Custom Fixed-Size Memory Pool Allocator for Network Buffers
**Question:** Design a fixed-size memory pool to allocate 1024-byte network frame buffers without calling global `malloc`/`free`.

```cpp
#include <vector>
#include <cstddef>
#include <new>

template <size_t BlockSize, size_t BlockCount>
class MemoryPool {
    struct Node { Node* next; };
    alignas(std::max_align_t) char storage_[BlockSize * BlockCount];
    Node* free_list_ = nullptr;

public:
    MemoryPool() {
        for (size_t i = 0; i < BlockCount; ++i) {
            Node* curr = reinterpret_cast<Node*>(storage_ + i * BlockSize);
            curr->next = free_list_;
            free_list_ = curr;
        }
    }

    void* allocate() {
        if (!free_list_) throw std::bad_alloc();
        Node* block = free_list_;
        free_list_ = free_list_->next;
        return block;
    }

    void deallocate(void* ptr) noexcept {
        if (!ptr) return;
        Node* block = static_cast<Node*>(ptr);
        block->next = free_list_;
        free_list_ = block;
    }
};
```
- **Time Complexity:** $O(1)$ constant time allocation and deallocation.

---

#### Q8. Memory Leak & Corruption Detection Tools
1. **AddressSanitizer (ASan / LSan)**: `-fsanitize=address -g` compiled runtime instrumentation. Intercepts allocations to detect use-after-free, buffer overflows, and memory leaks.
2. **Valgrind (Memcheck)**: Simulated CPU instruction runner. Checks uninitialized memory reads and memory leaks without recompilation.
3. **GDB Core Dump Analysis**: Load crash dumps via `gdb ./executable core` to inspect stack traces (`bt`) and memory buffers.

---

#### Q9. Dangling Pointers, Double Free, Use-After-Free & `nullptr` Safety
- **Dangling Pointer**: Pointer referencing memory that has already been deallocated (`free(p);`).
- **Use-After-Free**: Reading/writing through a dangling pointer (causes memory corruption / UB).
- **Double Free**: Calling `free(p)` twice on the same address (corrupts heap allocator metadata).
- **Safety**: Immediately set raw pointers to `nullptr` after deallocation (`free(p); p = nullptr;`). `delete nullptr;` is a guaranteed safe no-op.

---

#### Q10. Smart Pointers in Embedded C++ Systems
- **`std::unique_ptr`**: Single owner. Zero memory/runtime overhead ($sizeof(\text{unique\_ptr}) == sizeof(\text{raw pointer})$). Preferred for resource lifecycle management in embedded base stations.
- **`std::shared_ptr`**: Shared ownership. Allocates 16-byte control block + atomic ref-counting. High overhead on embedded ARM cores.
- **`std::weak_ptr`**: Non-owning observer used to break circular references.

---

### Category 2: Network Programming, Socket I/O & Protocols

#### Q11. TCP vs UDP Socket API Workflow
```
     TCP Server Workflow                    TCP Client Workflow
   ┌──────────────────────┐               ┌──────────────────────┐
   │ socket()             │               │ socket()             │
   │ bind()               │               ├──────────────────────┤
   │ listen()             │               │ connect() ───────────┼────┐
   │ accept() <───────────┼───────────────┤                      │    │
   ├──────────────────────┤ (3-Way Handshake)                    │    │
   │ read() / write()     │ <────────────>│ write() / read()     │    │
   │ close()              │               │ close()              │    │
   └──────────────────────┘               └──────────────────────┘    │
                                                                      ▼
```

- **TCP**: Connection-oriented, reliable stream, guaranteed order, flow control.
- **UDP**: Connectionless, unreliable datagrams, low latency, no ordering guarantees.

---

#### Q12. Non-Blocking Socket I/O & Linux `epoll` Event Loops

```cpp
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// epoll event loop sketch
void runEventLoop(int server_fd) {
    int epoll_fd = epoll_create1(0);
    epoll_event ev{}, events[64];
    ev.events = EPOLLIN | EPOLLET; // Edge-Triggered I/O
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    while (true) {
        int nfds = epoll_wait(epoll_fd, events, 64, -1);
        for (int i = 0; i < nfds; ++i) {
            // Handle active non-blocking socket reads/writes...
        }
    }
}
```

---

#### Q13. Network Byte Order (Big-Endian vs Little-Endian)
Network protocols (IP/TCP) transmit integers in **Big-Endian** (Network Byte Order). x86/ARM CPUs operate in **Little-Endian**.
- `htons()` / `htonl()`: Host to Network Short / Long.
- `ntohs()` / `ntohl()`: Network to Host Short / Long.

```cpp
#include <arpa/inet.h>
#include <cstdint>

uint32_t serializeHeader(uint32_t packet_id) {
    return htonl(packet_id); // Converts host endianness to big-endian
}
```

---

#### Q14. Network Packet Framing & Struct Packing (`#pragma pack(1)`)
Compilers insert alignment padding inside structs. Protocol headers sent across sockets MUST disable padding using `#pragma pack(1)`:

```cpp
#pragma pack(push, 1)
struct TelecomHeader {
    uint8_t  version;     // 1 byte
    uint16_t msg_type;    // 2 bytes
    uint32_t payload_len; // 4 bytes
}; // Exact size: 7 bytes (without packing, compiler adds padding to 8 bytes!)
#pragma pack(pop)
```

---

#### Q15. Custom Ring Buffer for Packet Stream Processing
**Question:** Implement a circular ring buffer to store incoming stream data from a non-blocking TCP socket.

```cpp
#include <vector>
#include <cstddef>
#include <algorithm>

class PacketRingBuffer {
    std::vector<char> buffer_;
    size_t head_ = 0; // Read pos
    size_t tail_ = 0; // Write pos
    size_t size_ = 0;
    size_t capacity_;

public:
    explicit PacketRingBuffer(size_t cap) : buffer_(cap), capacity_(cap) {}

    bool write(const char* data, size_t len) {
        if (capacity_ - size_ < len) return false; // Not enough space
        for (size_t i = 0; i < len; ++i) {
            buffer_[tail_] = data[i];
            tail_ = (tail_ + 1) % capacity_;
        }
        size_ += len;
        return true;
    }

    bool read(char* out, size_t len) {
        if (size_ < len) return false; // Not enough data
        for (size_t i = 0; i < len; ++i) {
            out[i] = buffer_[head_];
            head_ = (head_ + 1) % capacity_;
        }
        size_ -= len;
        return true;
    }
};
```

---

#### Q16. Zero-Copy Packet Buffer Management using `std::span` (C++20)
`std::span<const uint8_t>` allows parsing packet sub-headers without allocating memory or copying byte arrays:

```cpp
#include <span>
#include <cstdint>

void parseIpPayload(std::span<const uint8_t> packet) {
    if (packet.size() < 20) return; // IP Header minimum size
    auto payload = packet.subspan(20); // Zero-copy view of payload!
}
```

---

#### Q17. Handling UDP Packet Loss & Retransmission Buffering
- **UDP Datagrams**: Unreliable and out-of-order.
- **Solution**: Applications add a sequence number (`uint32_t seq`) to every packet. The receiver buffers incoming packets in a `std::map<uint32_t, Packet>` and requests retransmissions for missing gaps.

---

#### Q18. OSI 7-Layer Model & Telecommunications Packet Flow
1. **Physical Layer**: Bit transmission over fiber/radio RF.
2. **Data Link Layer**: Ethernet MAC framing.
3. **Network Layer**: IP routing, packet IP headers.
4. **Transport Layer**: TCP / UDP port multiplexing.
5. **Session Layer**: RPC / GTP tunnel session establishment.
6. **Presentation Layer**: SSL/TLS encryption, Data Serialization.
7. **Application Layer**: SIP, HTTP/2, 5G NAS/RRC protocols.

---

#### Q19. Network Protocol Packet Serializer / Deserializer
```cpp
#include <vector>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>

struct ControlMessage {
    uint16_t cmd_id;
    uint32_t sequence;

    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> buf(6);
        uint16_t net_cmd = htons(cmd_id);
        uint32_t net_seq = htonl(sequence);
        std::memcpy(buf.data(), &net_cmd, 2);
        std::memcpy(buf.data() + 2, &net_seq, 4);
        return buf;
    }

    static ControlMessage deserialize(const uint8_t* buf) {
        uint16_t net_cmd;
        uint32_t net_seq;
        std::memcpy(&net_cmd, buf, 2);
        std::memcpy(&net_seq, buf + 2, 4);
        return {ntohs(net_cmd), ntohl(net_seq)};
    }
};
```

---

#### Q20. Handling Partial Reads / Writes in Non-Blocking Sockets
Non-blocking `read()` / `write()` calls may return `EAGAIN` or `EWOULDBLOCK`, indicating partial data transfer:
- **`write()`**: Must loop and track bytes written (`total_sent += n`). If `EAGAIN` occurs, store un-sent bytes in a buffer and wait for `EPOLLOUT`.
- **`read()`**: Must loop until `EAGAIN` to ensure edge-triggered `epoll` drains all socket data.

---

### Category 3: Concurrency, IPC & Real-Time Operating Systems (RTOS)

#### Q21. Inter-Process Communication (IPC) Mechanisms
1. **Shared Memory (`shmget` / `mmap`)**: Fastest IPC ($O(1)$ zero-copy). Requires semaphores/mutexes for synchronization.
2. **POSIX Message Queues (`mq_open`)**: Kernel-managed prioritized message passing.
3. **Unix Domain Sockets (`AF_UNIX`)**: High-performance local bidirectional IPC using socket APIs.
4. **Pipes & FIFOs**: Unidirectional byte stream IPC between parent-child processes.

---

#### Q22. Mutex vs Counting Semaphore vs Binary Semaphore
- **`std::mutex`**: Ownership-based lock. Only the thread that locked the mutex can unlock it.
- **Binary Semaphore**: Signal mechanism (value 0 or 1). Any thread can post/signal it.
- **Counting Semaphore (`std::counting_semaphore`)**: Manages resource count $N$. Multiple threads can acquire tokens simultaneously.

```cpp
#include <semaphore>
#include <thread>

// Counting semaphore allowing max 3 simultaneous cell-tower connections
std::counting_semaphore<3> radio_resources(3);

void connectTower() {
    radio_resources.acquire(); // Decrements count (blocks if 0)
    // Transmit data...
    radio_resources.release(); // Increments count
}
```

---

#### Q23. Priority Inversion & Priority Inheritance in RTOS
- **Priority Inversion**: A high-priority thread ($T_{\text{high}}$) is blocked waiting for a resource held by a low-priority thread ($T_{\text{low}}$), while a medium-priority thread ($T_{\text{med}}$) preempts $T_{\text{low}}$, delaying $T_{\text{high}}$ indefinitely!
- **Solution (Priority Inheritance Protocol)**: When $T_{\text{high}}$ blocks on a mutex held by $T_{\text{low}}$, the RTOS temporarily elevates $T_{\text{low}}$'s priority to match $T_{\text{high}}$ until $T_{\text{low}}$ releases the mutex.

---

#### Q24. Thread-Safe Bounded Queue for Telemetry Logs
```cpp
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class SafeTelemetryQueue {
    std::queue<T> q_;
    size_t max_size_;
    std::mutex mtx_;
    std::condition_variable cv_push_, cv_pop_;

public:
    explicit SafeTelemetryQueue(size_t max_size) : max_size_(max_size) {}

    void push(T item) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_push_.wait(lock, [&]{ return q_.size() < max_size_; });
        q_.push(std::move(item));
        cv_pop_.notify_one();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_pop_.wait(lock, [&]{ return !q_.empty(); });
        T item = std::move(q_.front());
        q_.pop();
        cv_push_.notify_one();
        return item;
    }
};
```

---

#### Q25. Deadlock Prevention & `std::scoped_lock`
Prevent deadlocks by ordering lock acquisitions statically or using `std::scoped_lock` (C++17):

```cpp
std::mutex m1, m2;
void safeTask() {
    std::scoped_lock lock(m1, m2); // Deadlock-free multi-mutex acquisition
}
```

---

#### Q26. `std::atomic` vs `std::mutex` in Multi-Core Base Stations
- `std::atomic`: Lock-free, zero thread context switching. Used for counters, packet metrics, and atomic flag flags.
- `std::mutex`: Preferred for complex multi-variable state updates or I/O operations.

---

#### Q27. Reader-Writer Lock (`std::shared_mutex`) for Routing Tables
Routing tables are read constantly by thousands of packet threads, but updated rarely:

```cpp
#include <shared_mutex>
#include <unordered_map>
#include <string>

class RoutingTable {
    std::unordered_map<std::string, std::string> table_;
    mutable std::shared_mutex rw_mtx_;

public:
    std::string lookup(const std::string& ip) const {
        std::shared_lock<std::shared_mutex> lock(rw_mtx_); // Multiple readers allowed
        auto it = table_.find(ip);
        return (it != table_.end()) ? it->second : "";
    }

    void updateRoute(const std::string& ip, const std::string& gateway) {
        std::unique_lock<std::shared_mutex> lock(rw_mtx_); // Exclusive writer lock
        table_[ip] = gateway;
    }
};
```

---

#### Q28. Condition Variables & Spurious Wakeup Predicate Loop
- **Spurious Wakeup**: OS kernel signals condition variables without explicit notifications.
- **Rule**: Always wrap `cv.wait()` inside a predicate lambda: `cv.wait(lock, [&]{ return ready; });`.

---

#### Q29. Worker Thread Pool Architecture Sketch
```cpp
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

class WorkerThreadPool {
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    explicit WorkerThreadPool(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            threads_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx_);
                        cv_.wait(lock, [this]{ return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    ~WorkerThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : threads_) if (t.joinable()) t.join();
    }
};
```

---

#### Q30. `volatile` vs `std::atomic` in Hardware Register Access
- **`volatile`**: Tells compiler memory location can change externally (e.g. Memory-Mapped I/O hardware register). Prevents compiler instruction caching. **DOES NOT SYNCHRONIZE MULTITHREADING!**
- **`std::atomic`**: Guarantees atomic operations, memory barriers, and thread synchronization.

---

### Category 4: OOP, Language Internals & Modern C++

#### Q31. Virtual Functions, `vtable`/`vptr` & Dynamic Dispatch
- `vptr`: Hidden pointer placed at offset 0 of objects with virtual methods.
- `vtable`: Static array of virtual function pointers per class.
- Calling `ptr->func()` dereferences `ptr->vptr`, loads `vtable[slot]`, and executes an indirect jump (`call rax`).

---

#### Q32. Virtual Destructors & Abstract Base Classes
Deleting a derived object via a base pointer (`Base* p = new Derived(); delete p;`) when `Base` lacks a `virtual` destructor causes **Undefined Behavior** (derived destructor never executes, leaking resources!).

```cpp
class NetworkHandler { // Abstract Base Class
public:
    virtual ~NetworkHandler() = default; // MUST be virtual!
    virtual void handlePacket() = 0;     // Pure virtual function
};
```

---

#### Q33. Object Slicing & Pass-by-Reference
Assigning or passing a derived object by value into a base class variable slices off the derived members and resets `vptr` to the base `vtable`.
- **Fix**: Always pass polymorphic objects by pointer or `const Reference&`.

---

#### Q34. Rule of Zero / Three / Five
- **Rule of Zero**: Use standard RAII containers. Declare zero custom destructors or copy/move constructors.
- **Rule of Five**: If custom memory management is needed, declare all 5: Destructor, Copy Constructor, Copy Assignment, Move Constructor, Move Assignment.

---

#### Q35. Move Semantics (`std::move`, `std::forward`, RVO/NRVO)
- `std::move`: Casts argument to an rvalue reference (`T&&`). Does NOT move bytes by itself.
- `std::forward`: Conditional cast for perfect forwarding in templates.
- RVO (Return Value Optimization): Compiler constructs local return objects directly in caller memory frame.

---

#### Q36. Const Correctness & `mutable` Keyword
- `const` member functions promise not to modify object data members.
- `mutable` member variables (e.g. mutexes, log counters) can be modified inside `const` methods.

---

#### Q37. C++ Casting Operators
1. `static_cast`: Safe compile-time conversions.
2. `dynamic_cast`: Runtime checked downcast using RTTI.
3. `const_cast`: Casts away `const`/`volatile` qualifiers.
4. `reinterpret_cast`: Low-level reinterpretation of raw bit patterns.

---

#### Q38. SFINAE vs C++20 Concepts
C++20 Concepts replace cryptic SFINAE templates with human-readable constraints:

```cpp
#include <concepts>

template <std::integral T>
T add(T a, T b) { return a + b; } // Constrained template!
```

---

#### Q39. `constexpr` vs `consteval` vs `constinit`
- `constexpr`: Evaluated at compile time if inputs are constant; runtime otherwise.
- `consteval`: Immediate function (GUARANTEED compile-time ONLY).
- `constinit`: Guarantees compile-time initialization for static variables.

---

#### Q40. Monadic Error Handling (`std::expected` in C++23)
```cpp
#include <expected>
#include <string>

std::expected<int, std::string> parsePort(std::string_view str) {
    if (str.empty()) return std::unexpected("Empty port string");
    return 8080;
}
```

---

### Category 5: Data Structures, Algorithmic Coding & Systems Gotchas

#### Q41. Reverse a Singly Linked List (Iterative)
```cpp
struct ListNode {
    int val;
    ListNode* next;
    ListNode(int x) : val(x), next(nullptr) {}
};

ListNode* reverseList(ListNode* head) {
    ListNode* prev = nullptr;
    ListNode* curr = head;
    while (curr) {
        ListNode* next_node = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next_node;
    }
    return prev;
}
```
- **Time Complexity:** $O(N)$, **Space Complexity:** $O(1)$.

---

#### Q42. Detect & Remove Cycle in Linked List (Floyd's Algorithm)
```cpp
bool hasCycle(ListNode* head) {
    ListNode* slow = head;
    ListNode* fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return true;
    }
    return false;
}
```

---

#### Q43. Design LRU Cache ($O(1)$ Operations)
```cpp
#include <unordered_map>
#include <list>
#include <utility>

class LRUCache {
    size_t cap_;
    using Pair = std::pair<int, int>;
    std::list<Pair> list_;
    std::unordered_map<int, std::list<Pair>::iterator> map_;

public:
    explicit LRUCache(int capacity) : cap_(capacity) {}

    int get(int key) {
        auto it = map_.find(key);
        if (it == map_.end()) return -1;
        list_.splice(list_.begin(), list_, it->second); // Move to front
        return it->second->second;
    }

    void put(int key, int value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }
        if (map_.size() >= cap_) {
            int old_key = list_.back().first;
            map_.erase(old_key);
            list_.pop_back();
        }
        list_.push_front({key, value});
        map_[key] = list_.begin();
    }
};
```

---

#### Q44. `std::map` vs `std::unordered_map` Performance & Rehash
- `std::map`: Red-Black tree ($O(\log N)$ lookup). Memory scattered across nodes.
- `std::unordered_map`: Bucket array + linked list chains. $O(1)$ average lookup. Triggering `rehash()` when load factor $> 1.0$ re-allocates bucket array and re-hashes all keys ($O(N)$ spike!).

---

#### Q45. Iterator Invalidation Rules Summary
- `std::vector`: `push_back`/`insert` invalidates ALL iterators if reallocation occurs.
- `std::deque`: `push_front`/`push_back` invalidates iterators (references remain valid).
- `std::list` / `std::map`: `insert` NEVER invalidates iterators or references. `erase` invalidates only erased element.

---

#### Q46. Remove-Erase Idiom Mechanics
```cpp
std::vector<int> vec = {1, 2, 3, 4, 5, 6};
vec.erase(std::remove_if(vec.begin(), vec.end(), [](int x) { return x % 2 != 0; }), vec.end());
```

---

#### Q47. Find First Non-Repeating Character in Stream
```cpp
#include <string>
#include <vector>
#include <queue>

char firstUniqChar(std::string s) {
    std::vector<int> count(256, 0);
    std::queue<char> q;
    for (char c : s) {
        count[static_cast<unsigned char>(c)]++;
        q.push(c);
        while (!q.empty() && count[static_cast<unsigned char>(q.front())] > 1) {
            q.pop();
        }
    }
    return q.empty() ? ' ' : q.front();
}
```

---

#### Q48. Struct Alignment & Padding Rules
Ordering struct fields from largest alignment to smallest alignment eliminates compiler padding bytes (saving up to 50% memory size).

---

#### Q49. False Sharing Mitigation in Multi-Core Network Base Stations
Place shared atomic variables accessed by different CPU cores on separate 64-byte aligned memory lines using `alignas(64)`.

---

#### Q50. Static Initialization Order Fiasco Solution
Function-local static variables (Meyer's Singleton) guarantee thread-safe first-call initialization, preventing cross-translation unit initialization order bugs.

---

#### Q51. Modern C++20 Ranges & Views Lazy Evaluation
```cpp
#include <ranges>
#include <vector>

std::vector<int> nums = {1, 2, 3, 4, 5};
auto view = nums | std::views::filter([](int x) { return x % 2 == 0; });
```

---

#### Q52. Master Nokia C++ Interview Gotcha Table

| Gotcha | The Trap | The Fix |
|--------|----------|---------|
| Non-virtual destructor | Derived destructor omitted during polymorphic delete | Always declare base destructor as `virtual` |
| Array decay `sizeof` | `sizeof(arr)` inside function returns pointer size (8 bytes) | Pass size explicitly or use `std::span` / `std::array` |
| Erasing inside loop | `v.erase(it)` invalidates `it`, causing `++it` UB | Reassign: `it = v.erase(it);` |
| Thread `join()` omission | `std::thread` destructor calls `std::terminate()` if joinable | Call `join()`, `detach()`, or use `std::jthread` |
| Non-blocking socket read | Incomplete socket data read on single `read()` call | Loop `read()` until `EAGAIN` / `EWOULDBLOCK` |
| Bitwise precedence | `a & b == 0` evaluates as `a & (b == 0)` due to `==` precedence | Wrap bitwise operations in parens: `(a & b) == 0` |
| Un-packed protocol struct | Compiler inserts padding bytes into socket byte streams | Use `#pragma pack(1)` for binary protocol headers |
