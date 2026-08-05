# Adobe — C++ Interview Questions & Systems Reference

> **Focus:** Custom Memory Management, Document Engine Architecture (Acrobat/InDesign), Photoshop Graphics & Filter Pipelines, Thread Safety, Undo/Redo Systems, and Core Modern C++

---

## Overview of Adobe C++ Interviews

Adobe software engineering rounds heavily test **low-level C++ mechanics, custom data structure implementation, document system design, and algorithms**:
- **Memory & Pointers**: Building custom `shared_ptr`, custom `String` classes with deep copy / Move semantics, preventing cyclic reference leaks (`weak_ptr`), placement `new`, and memory alignment.
- **Document & Graphics Systems**: Undo/Redo architectures using the **Command Pattern**, LRU asset caching ($O(1)$ operations), Flyweight glyph rendering in PDF engines, and Quadtree spatial partitioning.
- **Concurrency & Parallel Filtering**: Thread-safe Producer-Consumer queues, Reader-Writer locks (`std::shared_mutex`) for document asset editing, Thread Pools, and multi-core image filter rendering (`std::async`).
- **Algorithms & Data Structures**: String parsing (`atoi`, KMP algorithm), 2D Matrix transformations, Binary Trees (LCA, Right View, Spiral View), Linked List manipulations, and LIS / Russian Doll Envelope layer stacking.

---

## 52 Interview Questions & Solutions

### Category 1: Memory Management, Smart Pointers & Custom Allocators

#### Q1. Implement a Custom Thread-Safe `shared_ptr` from Scratch
**Question:** Implement a simplified, thread-safe `SharedPtr<T>` with atomic reference counting.

```cpp
#include <atomic>
#include <utility>
#include <cstddef>

template <typename T>
class SharedPtr {
    struct ControlBlock {
        std::atomic<size_t> ref_count{1};
        std::atomic<size_t> weak_count{0};
    };

    T* ptr_ = nullptr;
    ControlBlock* cb_ = nullptr;

public:
    constexpr SharedPtr() noexcept = default;

    explicit SharedPtr(T* ptr) : ptr_(ptr) {
        if (ptr_) cb_ = new ControlBlock();
    }

    ~SharedPtr() { release(); }

    // Copy Constructor
    SharedPtr(const SharedPtr& other) noexcept : ptr_(other.ptr_), cb_(other.cb_) {
        if (cb_) cb_->ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    // Move Constructor
    SharedPtr(SharedPtr&& other) noexcept : ptr_(other.ptr_), cb_(other.cb_) {
        other.ptr_ = nullptr;
        other.cb_ = nullptr;
    }

    // Copy Assignment
    SharedPtr& operator=(const SharedPtr& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            cb_ = other.cb_;
            if (cb_) cb_->ref_count.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }

    // Move Assignment
    SharedPtr& operator=(SharedPtr&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            cb_ = other.cb_;
            other.ptr_ = nullptr;
            other.cb_ = nullptr;
        }
        return *this;
    }

    T* get() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    size_t use_count() const noexcept { return cb_ ? cb_->ref_count.load() : 0; }

private:
    void release() noexcept {
        if (cb_ && cb_->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete ptr_;
            if (cb_->weak_count.load(std::memory_order_relaxed) == 0) {
                delete cb_;
            }
            ptr_ = nullptr;
            cb_ = nullptr;
        }
    }
};
```

---

#### Q2. Implement a Custom `String` Class (Rule of Five & Deep Copy)
**Question:** Write a production-ready C++ `String` class adhering to the Rule of Five.

```cpp
#include <iostream>
#include <cstring>
#include <utility>

class String {
    char* data_;
    size_t size_;

public:
    // Default Constructor
    String() : data_(new char[1]{'\0'}), size_(0) {}

    // C-String Constructor
    String(const char* str) {
        if (!str) {
            data_ = new char[1]{'\0'};
            size_ = 0;
        } else {
            size_ = std::strlen(str);
            data_ = new char[size_ + 1];
            std::strcpy(data_, str);
        }
    }

    // Destructor
    ~String() { delete[] data_; }

    // Copy Constructor (Deep Copy)
    String(const String& other) : size_(other.size_) {
        data_ = new char[size_ + 1];
        std::strcpy(data_, other.data_);
    }

    // Move Constructor
    String(String&& other) noexcept : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }

    // Copy Assignment (Copy-and-Swap Idiom)
    String& operator=(String other) noexcept {
        swap(*this, other);
        return *this;
    }

    friend void swap(String& a, String& b) noexcept {
        using std::swap;
        swap(a.data_, b.data_);
        swap(a.size_, b.size_);
    }

    const char* c_str() const noexcept { return data_ ? data_ : ""; }
    size_t length() const noexcept { return size_; }
};
```

---

#### Q3. `new`/`delete` vs `malloc`/`free` vs Placement `new`
- **`malloc`/`free`**: Raw byte memory allocation from C heap. No object constructor/destructor execution.
- **`new`/`delete`**: C++ operators that allocate dynamic memory AND execute class constructors/destructors. Throws `std::bad_alloc`.
- **Placement `new`**: Constructs objects at pre-allocated memory addresses (`new (addr) T(args)`). Does not perform heap allocation. Teardown requires explicit destructor calls (`ptr->~T()`).

---

#### Q4. Cyclic Reference Memory Leaks & `std::weak_ptr`
- **Problem**: When two objects contain `std::shared_ptr` pointing to each other, their reference counts remain 1 forever, leaking memory upon scope exit.
- **Solution**: Break the cycle by declaring non-owning references as `std::weak_ptr`.

```cpp
#include <memory>

struct LayerNode {
    std::shared_ptr<LayerNode> child;
    std::weak_ptr<LayerNode> parent; // Prevents cycle leak!
};
```

---

#### Q5. Why `std::make_shared` is Preferred Over `shared_ptr<T>(new T)`
1. **Single Allocation**: `std::make_shared` allocates object payload `T` and control block together in **1 contiguous memory block** (vs 2 separate allocations for raw `new`).
2. **Exception Safety**: Avoids memory leaks if another function argument evaluation throws before `shared_ptr` constructor finishes.

---

#### Q6. Stack vs Heap & Memory Leak Detection Tools
- **Stack**: Fast contiguous allocation ($O(1)$ stack pointer shift). Automatically freed on function exit.
- **Heap**: Dynamic memory allocations via OS `brk`/`mmap`. Requires explicit free.
- **Detection Tools**: AddressSanitizer (`-fsanitize=address -g`) and `Valgrind` (`valgrind --leak-check=full`).

---

#### Q7. Custom Fixed-Block Memory Allocator for Document Nodes
```cpp
#include <cstddef>
#include <new>

template <size_t NodeSize, size_t PoolCount>
class DocumentNodeAllocator {
    struct Block { Block* next; };
    alignas(std::max_align_t) char memory_[NodeSize * PoolCount];
    Block* free_head_ = nullptr;

public:
    DocumentNodeAllocator() {
        for (size_t i = 0; i < PoolCount; ++i) {
            Block* curr = reinterpret_cast<Block*>(memory_ + i * NodeSize);
            curr->next = free_head_;
            free_head_ = curr;
        }
    }

    void* allocate() {
        if (!free_head_) throw std::bad_alloc();
        Block* block = free_head_;
        free_head_ = free_head_->next;
        return block;
    }

    void deallocate(void* ptr) noexcept {
        if (!ptr) return;
        Block* block = static_cast<Block*>(ptr);
        block->next = free_head_;
        free_head_ = block;
    }
};
```

---

#### Q8. Dangling Pointers, Wild Pointers & `nullptr` Safety
- **Dangling Pointer**: Points to deallocated memory (`delete p;`). Access causes Use-After-Free UB.
- **Wild Pointer**: Uninitialized raw pointer (`int* p;`).
- **Fix**: Initialize pointers to `nullptr` and re-assign `p = nullptr;` immediately after deletion (`delete nullptr;` is a safe no-op).

---

#### Q9. Explicit Destructor Invocations & Aligned Buffers
When using pre-allocated byte buffers, calling `delete ptr;` on placement `new` objects causes Undefined Behavior. Call `ptr->~T()` explicitly instead.

---

#### Q10. Custom Deleters in `std::unique_ptr` for OS Graphics Handles
```cpp
#include <memory>
#include <cstdio>

auto make_scoped_file(const char* filename) {
    return std::unique_ptr<FILE, decltype(&std::fclose)>(
        std::fopen(filename, "rb"), &std::fclose
    );
}
```

---

### Category 2: Document Processing, Design Patterns & Systems Architecture

#### Q11. Undo/Redo System Design (Command Pattern)
**Question:** Design an Undo/Redo stack manager for a Document Editor (e.g. Photoshop / Acrobat).

```cpp
#include <vector>
#include <memory>
#include <string>

class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

class TextInsertCommand : public Command {
    std::string& doc_text_;
    std::string added_text_;
    size_t pos_;

public:
    TextInsertCommand(std::string& doc, std::string text, size_t pos)
        : doc_text_(doc), added_text_(std::move(text)), pos_(pos) {}

    void execute() override { doc_text_.insert(pos_, added_text_); }
    void undo() override { doc_text_.erase(pos_, added_text_.length()); }
};

class UndoManager {
    std::vector<std::unique_ptr<Command>> undo_stack_;
    std::vector<std::unique_ptr<Command>> redo_stack_;

public:
    void executeCommand(std::unique_ptr<Command> cmd) {
        cmd->execute();
        undo_stack_.push_back(std::move(cmd));
        redo_stack_.clear(); // Clear redo stack on new operation
    }

    void undo() {
        if (undo_stack_.empty()) return;
        auto cmd = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        cmd->undo();
        redo_stack_.push_back(std::move(cmd));
    }

    void redo() {
        if (redo_stack_.empty()) return;
        auto cmd = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        cmd->execute();
        undo_stack_.push_back(std::move(cmd));
    }
};
```

---

#### Q12. Design an LRU Cache for Layer/Asset Rendering ($O(1)$ Operations)
```cpp
#include <unordered_map>
#include <list>
#include <utility>

class LRUCache {
    size_t capacity_;
    using Pair = std::pair<int, int>;
    std::list<Pair> items_;
    std::unordered_map<int, std::list<Pair>::iterator> map_;

public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    int get(int key) {
        auto it = map_.find(key);
        if (it == map_.end()) return -1;
        items_.splice(items_.begin(), items_, it->second);
        return it->second->second;
    }

    void put(int key, int value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            items_.splice(items_.begin(), items_, it->second);
            return;
        }
        if (map_.size() >= capacity_) {
            int old_key = items_.back().first;
            map_.erase(old_key);
            items_.pop_back();
        }
        items_.push_front({key, value});
        map_[key] = items_.begin();
    }
};
```

---

#### Q13. Flyweight Design Pattern for Document Character Rendering
Saves memory in PDF rendering engines by sharing intrinsic glyph properties (font, size, texture) across millions of rendered characters.

```cpp
#include <unordered_map>
#include <memory>
#include <string>

struct GlyphIntrinsic {
    char character;
    std::string font_family;
    int size;
};

class GlyphFactory {
    std::unordered_map<char, std::shared_ptr<GlyphIntrinsic>> pool_;
public:
    std::shared_ptr<GlyphIntrinsic> getGlyph(char c) {
        if (!pool_.count(c)) {
            pool_[c] = std::make_shared<GlyphIntrinsic>(GlyphIntrinsic{c, "Arial", 12});
        }
        return pool_[c];
    }
};
```

---

#### Q14. Quadtree Bounding Box Partitioning for Document Canvas
Partitions 2D canvas space into 4 quadrants (`NW`, `NE`, `SW`, `SE`) to accelerate collision detection and graphic object rendering queries from $O(N)$ to $O(\log N)$.

---

#### Q15. Factory Pattern for Document Exporters (PDF, PNG, SVG)
```cpp
#include <memory>
#include <string>

class DocumentExporter { public: virtual ~DocumentExporter() = default; virtual void exportDoc() = 0; };
class PDFExporter : public DocumentExporter { public: void exportDoc() override {} };
class PNGExporter : public DocumentExporter { public: void exportDoc() override {} };

class ExporterFactory {
public:
    static std::unique_ptr<DocumentExporter> create(const std::string& type) {
        if (type == "pdf") return std::make_unique<PDFExporter>();
        if (type == "png") return std::make_unique<PNGExporter>();
        return nullptr;
    }
};
```

---

#### Q16. Thread-Safe Singleton Pattern (Meyer's Singleton)
```cpp
class ApplicationConfig {
public:
    static ApplicationConfig& getInstance() {
        static ApplicationConfig instance; // Thread-safe in C++11!
        return instance;
    }
private:
    ApplicationConfig() = default;
};
```

---

#### Q17. Observer Pattern for Document Layer Events
```cpp
#include <vector>
#include <algorithm>

class LayerObserver { public: virtual ~LayerObserver() = default; virtual void onLayerModified() = 0; };

class DocumentLayer {
    std::vector<LayerObserver*> observers_;
public:
    void attach(LayerObserver* obs) { observers_.push_back(obs); }
    void notify() { for (auto* obs : observers_) obs->onLayerModified(); }
};
```

---

#### Q18. Pimpl Idiom for SDK ABI Stability in Photoshop Plug-Ins
Hides implementation details inside a `.cpp` file to preserve binary ABI layout compatibility when SDKs update.

---

#### Q19. Thread-Safe Asynchronous Logger Class
```cpp
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <fstream>

class AsyncLogger {
    std::queue<std::string> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread worker_;
    bool stop_ = false;

public:
    AsyncLogger() {
        worker_ = std::thread([this] {
            std::ofstream file("app.log");
            while (true) {
                std::string msg;
                {
                    std::unique_lock<std::mutex> lock(mtx_);
                    cv_.wait(lock, [this]{ return stop_ || !queue_.empty(); });
                    if (stop_ && queue_.empty()) return;
                    msg = std::move(queue_.front());
                    queue_.pop();
                }
                file << msg << "\n";
            }
        });
    }

    void log(std::string msg) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(msg));
        }
        cv_.notify_one();
    }

    ~AsyncLogger() {
        { std::lock_guard<std::mutex> lock(mtx_); stop_ = true; }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
};
```

---

#### Q20. Flatten Multilevel Doubly Linked List (LeetCode 430)
**Question:** Flatten a multilevel doubly linked list representing document DOM nodes.

```cpp
struct Node {
    int val;
    Node *prev, *next, *child;
};

Node* flatten(Node* head) {
    if (!head) return nullptr;
    Node* curr = head;
    while (curr) {
        if (curr->child) {
            Node* next_node = curr->next;
            Node* child_tail = flatten(curr->child);
            
            curr->next = curr->child;
            curr->child->prev = curr;
            curr->child = nullptr;
            
            Node* tail = curr->next;
            while (tail->next) tail = tail->next;
            
            tail->next = next_node;
            if (next_node) next_node->prev = tail;
        }
        curr = curr->next;
    }
    return head;
}
```

---

### Category 3: Concurrency, Multithreading & Async Filter Pipelines

#### Q21. Thread-Safe Bounded Queue for Image Rendering
```cpp
#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class BoundedQueue {
    std::queue<T> q_;
    size_t capacity_;
    std::mutex mtx_;
    std::condition_variable cv_push_, cv_pop_;

public:
    explicit BoundedQueue(size_t cap) : capacity_(cap) {}

    void push(T item) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_push_.wait(lock, [&]{ return q_.size() < capacity_; });
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

#### Q22. Reader-Writer Lock (`std::shared_mutex`) for Document Asset Reading vs Editing
```cpp
#include <shared_mutex>

class SharedDocumentAsset {
    mutable std::shared_mutex mtx_;
    int asset_data_ = 0;

public:
    int readData() const {
        std::shared_lock<std::shared_mutex> lock(mtx_); // Multiple concurrent readers
        return asset_data_;
    }

    void editData(int val) {
        std::unique_lock<std::shared_mutex> lock(mtx_); // Exclusive writer lock
        asset_data_ = val;
    }
};
```

---

#### Q23. Parallel Image Processing using `std::async`
```cpp
#include <future>
#include <vector>

double processTile(int tile_id) { return tile_id * 1.5; }

void processImageInParallel() {
    std::vector<std::future<double>> futures;
    for (int i = 0; i < 4; ++i) {
        futures.push_back(std::async(std::launch::async, processTile, i));
    }
    for (auto& f : futures) {
        double result = f.get();
    }
}
```

---

#### Q24. Mutex vs Semaphore vs Atomics in Multi-Core Image Filtering
- `std::atomic`: Ultra-fast lock-free operations for pixel counters ($O(1)$ cycles).
- `std::mutex`: Exclusive locking for shared image buffer updates.
- `std::counting_semaphore`: Limits max concurrent worker threads executing heavy GPU/CPU filters.

---

#### Q25. Condition Variables & Spurious Wakeups
Kernel interrupts can wake waiting threads without explicit `notify()` calls.
- **Rule**: Always wrap `cv.wait(lock, [&]{ return predicate; });` inside a boolean check.

---

#### Q26. Deadlock Prevention & `std::scoped_lock`
```cpp
std::mutex mtx1, mtx2;
void safeTask() {
    std::scoped_lock lock(mtx1, mtx2); // Locks both mutexes atomically without deadlock!
}
```

---

#### Q27. `std::atomic` Memory Orderings
- `memory_order_relaxed`: Atomic operation without thread synchronization fences.
- `memory_order_acquire` / `release`: Synchronizes shared memory reads/writes between threads.
- `memory_order_seq_cst`: Strict sequential ordering across all CPU cores.

---

#### Q28. False Sharing Prevention (`alignas(64)`)
```cpp
struct alignas(64) WorkerThreadStats {
    std::atomic<uint64_t> processed_pixels{0};
}; // Prevents CPU L1 cache line bouncing across multi-core image filters
```

---

#### Q29. Thread Pool Architecture for Image Filter Pipelines
```cpp
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>

class ThreadPool {
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;

public:
    explicit ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
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

    ~ThreadPool() {
        { std::unique_lock<std::mutex> lock(mtx_); stop_ = true; }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }
};
```

---

#### Q30. `volatile` vs `std::atomic`
- `volatile`: Memory-Mapped Hardware I/O register access. Prevents compiler optimization. Does NOT synchronize multithreaded operations!
- `std::atomic`: Guarantees thread synchronization, lock-free operations, and memory barriers.

---

### Category 4: Data Structures, Algorithms & Image Processing

#### Q31. String to Integer (`atoi`) with Edge Cases & Overflow
```cpp
#include <string>
#include <climits>

int myAtoi(std::string s) {
    int i = 0, n = s.length(), sign = 1;
    long result = 0;

    while (i < n && s[i] == ' ') i++; // Skip leading whitespace
    if (i < n && (s[i] == '+' || s[i] == '-')) {
        sign = (s[i] == '-') ? -1 : 1;
        i++;
    }

    while (i < n && s[i] >= '0' && s[i] <= '9') {
        result = result * 10 + (s[i] - '0');
        if (result * sign >= INT_MAX) return INT_MAX;
        if (result * sign <= INT_MIN) return INT_MIN;
        i++;
    }
    return result * sign;
}
```

---

#### Q32. KMP Substring Search Algorithm
```cpp
#include <string>
#include <vector>

std::vector<int> buildLPS(const std::string& pattern) {
    int m = pattern.length();
    std::vector<int> lps(m, 0);
    int len = 0, i = 1;
    while (i < m) {
        if (pattern[i] == pattern[len]) {
            lps[i++] = ++len;
        } else {
            if (len != 0) len = lps[len - 1];
            else lps[i++] = 0;
        }
    }
    return lps;
}

int kmpSearch(const std::string& text, const std::string& pattern) {
    int n = text.length(), m = pattern.length();
    if (m == 0) return 0;
    std::vector<int> lps = buildLPS(pattern);
    int i = 0, j = 0;
    while (i < n) {
        if (text[i] == pattern[j]) { i++; j++; }
        if (j == m) return i - j;
        else if (i < n && text[i] != pattern[j]) {
            if (j != 0) j = lps[j - 1];
            else i++;
        }
    }
    return -1;
}
```
- **Time Complexity:** $O(N + M)$ linear time search.

---

#### Q33. Rotate Image Matrix 90 Degrees In-Place
```cpp
#include <vector>
#include <algorithm>

void rotateImage(std::vector<std::vector<int>>& matrix) {
    int n = matrix.size();
    // 1. Transpose Matrix
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            std::swap(matrix[i][j], matrix[j][i]);
        }
    }
    // 2. Reverse each row
    for (int i = 0; i < n; ++i) {
        std::reverse(matrix[i].begin(), matrix[i].end());
    }
}
```

---

#### Q34. Spiral Matrix Traversal
```cpp
#include <vector>

std::vector<int> spiralOrder(const std::vector<std::vector<int>>& matrix) {
    std::vector<int> result;
    if (matrix.empty()) return result;
    int top = 0, bottom = matrix.size() - 1;
    int left = 0, right = matrix[0].size() - 1;

    while (top <= bottom && left <= right) {
        for (int j = left; j <= right; ++j) result.push_back(matrix[top][j]);
        top++;
        for (int i = top; i <= bottom; ++i) result.push_back(matrix[i][right]);
        right--;
        if (top <= bottom) {
            for (int j = right; j >= left; --j) result.push_back(matrix[bottom][j]);
            bottom--;
        }
        if (left <= right) {
            for (int i = bottom; i >= top; --i) result.push_back(matrix[i][left]);
            left++;
        }
    }
    return result;
}
```

---

#### Q35. Reverse a Singly Linked List
```cpp
struct ListNode {
    int val;
    ListNode* next;
    ListNode(int x) : val(x), next(nullptr) {}
};

ListNode* reverseList(ListNode* head) {
    ListNode *prev = nullptr, *curr = head;
    while (curr) {
        ListNode* next_node = curr->next;
        curr->next = prev;
        prev = curr;
        curr = next_node;
    }
    return prev;
}
```

---

#### Q36. Detect & Remove Cycle in Linked List
```cpp
bool hasCycle(ListNode* head) {
    ListNode *slow = head, *fast = head;
    while (fast && fast->next) {
        slow = slow->next;
        fast = fast->next->next;
        if (slow == fast) return true;
    }
    return false;
}
```

---

#### Q37. Lowest Common Ancestor (LCA) in Binary Tree
```cpp
struct TreeNode {
    int val;
    TreeNode *left, *right;
    TreeNode(int x) : val(x), left(nullptr), right(nullptr) {}
};

TreeNode* lowestCommonAncestor(TreeNode* root, TreeNode* p, TreeNode* q) {
    if (!root || root == p || root == q) return root;
    TreeNode* left = lowestCommonAncestor(root->left, p, q);
    TreeNode* right = lowestCommonAncestor(root->right, p, q);
    if (left && right) return root;
    return left ? left : right;
}
```

---

#### Q38. Binary Tree Right Side View
```cpp
#include <vector>

void dfsRightView(TreeNode* node, int depth, std::vector<int>& view) {
    if (!node) return;
    if (depth == view.size()) view.push_back(node->val);
    dfsRightView(node->right, depth + 1, view);
    dfsRightView(node->left, depth + 1, view);
}

std::vector<int> rightSideView(TreeNode* root) {
    std::vector<int> view;
    dfsRightView(root, 0, view);
    return view;
}
```

---

#### Q39. Longest Increasing Subsequence (LIS) / Russian Doll Envelopes
```cpp
#include <vector>
#include <algorithm>

int lengthOfLIS(const std::vector<int>& nums) {
    std::vector<int> tails;
    for (int x : nums) {
        auto it = std::lower_bound(tails.begin(), tails.end(), x);
        if (it == tails.end()) tails.push_back(x);
        else *it = x;
    }
    return tails.size();
}
```

---

#### Q40. Invert / Flip Binary Tree In-Place
```cpp
TreeNode* invertTree(TreeNode* root) {
    if (!root) return nullptr;
    std::swap(root->left, root->right);
    invertTree(root->left);
    invertTree(root->right);
    return root;
}
```

---

### Category 5: Object-Oriented Mechanics, Modern C++ & Systems Gotchas

#### Q41. Virtual Table (`vtable`) & `vptr` Mechanics
- `vptr`: Secret pointer stored at byte offset 0 of objects containing virtual functions.
- `vtable`: Static function pointer lookup array per class. Dynamic dispatch resolves `ptr->vptr[slot]()` at runtime.

---

#### Q42. Virtual Destructor Requirement
Executing `delete base_ptr;` when `base_ptr` points to a derived class without a `virtual` base destructor causes **Undefined Behavior** (derived destructor omitted, leaking resources!).

---

#### Q43. Object Slicing & Pass-by-Reference
Passing a derived object by value into a base class function parameter slices off the derived fields and resets `vptr` to the base class vtable. Pass by `const Base&` or `Base*` instead.

---

#### Q44. Move Semantics & `noexcept` Growth Requirement
- `std::move`: Casts argument to an rvalue reference (`T&&`).
- `noexcept` Move Constructor: Required for `std::vector` to use moves during capacity reallocation (otherwise copies for strong exception safety!).

---

#### Q45. Const Correctness & `mutable`
- `const` member functions promise not to modify object data fields.
- `mutable` allows variables (e.g. mutex locks, access counters) to be updated inside `const` methods.

---

#### Q46. Modern C++ Type Casting Operators
1. `static_cast`: Checked compile-time type conversions.
2. `dynamic_cast`: Safe RTTI downcasting for polymorphic objects.
3. `const_cast`: Adds/removes `const` or `volatile`.
4. `reinterpret_cast`: Raw bit-level pointer reinterpretation.

---

#### Q47. SFINAE vs C++20 Concepts
C++20 Concepts simplify template constraints:

```cpp
#include <concepts>
template <std::floating_point T>
T scale(T val, T factor) { return val * factor; }
```

---

#### Q48. `constexpr` vs `consteval` vs `constinit`
- `constexpr`: Evaluated at compile time if inputs are constant.
- `consteval`: Immediate function (MUST execute at compile time).
- `constinit`: Guarantees compile-time initialization for static/global variables.

---

#### Q49. Monadic Error Handling (`std::expected` C++23)
```cpp
#include <expected>
#include <string>

std::expected<double, std::string> computeOpacity(double val) {
    if (val < 0.0 || val > 1.0) return std::unexpected("Invalid opacity range");
    return val;
}
```

---

#### Q50. Struct Alignment & Padding Rules
Ordering struct data members from largest alignment to smallest minimizes compiler alignment padding bytes.

---

#### Q51. Modern C++20 Ranges Lazy Evaluation Pipeline
```cpp
#include <ranges>
#include <vector>

std::vector<int> layers = {1, 2, 3, 4, 5};
auto view = layers | std::views::filter([](int x) { return x % 2 == 0; });
```

---

#### Q52. Master Adobe C++ Interview Gotcha Table

| Gotcha | The Trap | The Fix |
|--------|----------|---------|
| Non-virtual base destructor | `delete base_ptr` misses derived destructor teardown | Always declare base class destructor as `virtual` |
| Cyclic `shared_ptr` loop | Memory leak: reference count never reaches 0 | Use `std::weak_ptr` for non-owning child/parent links |
| Pass derived object by value | Object slicing: derived data removed | Pass by reference (`const Base&`) or pointer (`Base*`) |
| Un-checked `atoi` overflow | Integer overflow crashes or wraps to `INT_MIN`/`INT_MAX` | Check overflow bounds (`long result`) during loop |
| Erasing `vector` in loop | `v.erase(it)` invalidates `it`, causing `++it` UB | Reassign iterator: `it = v.erase(it);` |
| Missing `noexcept` on move | `std::vector` falls back to slow copies on `realloc` | Mark move constructors as `noexcept` |
| `std::move` on return value | Prevents NRVO copy elision in return statements | Return local variables directly (`return obj;`) |
