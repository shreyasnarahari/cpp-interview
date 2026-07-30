# Theory & Mechanics: Exception Handling & noexcept

## 1. Zero-Cost Exception Model (Itanium ABI)

Modern compilers implement **zero-cost exceptions** — there is ZERO runtime overhead on the **happy path** (no exceptions thrown). All cost is paid on the throw path.

```
Happy path (no throw):
  - No extra instructions generated for try/catch
  - No if-checks after function calls (unlike error codes)
  - Function calls are identical to non-exception code
  - Cost: literally zero CPU cycles of overhead

Sad path (throw):
  1. Exception object is heap-allocated (~100-500 ns)
  2. Runtime walks the call stack using .eh_frame tables (~1-10 µs)
  3. For each frame, it calls destructors (stack unwinding)
  4. Finds the matching catch handler via LSDA (Language-Specific Data Area)
  5. Jumps to the landing pad (catch block)
  Total cost: ~10-100 µs per throw — extremely expensive
```

### How `.eh_frame` tables work:
```
The compiler generates metadata tables (stored in read-only data):

.eh_frame — Call Frame Information (CFI):
┌─────────────────────────────────────┐
│ Function: foo()                     │
│   PC range: 0x1000 - 0x1080        │
│   Unwind instructions:              │
│     - restore RBP from [RSP+8]     │
│     - restore return addr from [RSP]│
│   LSDA pointer → ...               │
├─────────────────────────────────────┤
│ Function: bar()                     │
│   ...                               │
└─────────────────────────────────────┘

LSDA — Landing pads (catch blocks):
┌─────────────────────────────────────┐
│ If PC in range [0x1020, 0x1040]:    │
│   → catch(std::runtime_error): jump │
│     to landing pad at 0x1060        │
│   → cleanup: call destructors       │
└─────────────────────────────────────┘
```

**Code size cost:** The `.eh_frame` tables increase binary size by ~10-20%, even though they add zero runtime cost on the happy path.

## 2. Stack Unwinding — Destruction Order

When an exception propagates, destructors are called in **reverse order of construction** for all fully-constructed objects in each stack frame:

```cpp
void risky() {
    A a;         // constructed 1st
    B b;         // constructed 2nd
    C c;         // constructed 3rd
    throw std::runtime_error("boom");
    // Stack unwinding destroys: ~C() → ~B() → ~A() (reverse order)
}

// CRITICAL: If a destructor throws during stack unwinding
// (while another exception is already in flight):
//   → std::terminate() is called immediately
//   → This is why destructors must NEVER throw
//   → Destructors are implicitly noexcept since C++11
```

## 3. Exception Safety Guarantees

| Guarantee | Promise | Example |
|---|---|---|
| **No-throw** | Operation never throws | `int::operator=`, destructors, `swap` |
| **Strong** | If it throws, state is rolled back completely | Copy-and-swap assignment |
| **Basic** | If it throws, invariants preserved, no leaks | Most STL operations |
| **No guarantee** | Anything can happen | Avoid writing code at this level |

### Copy-and-Swap Idiom (Strong Guarantee):
```cpp
class Widget {
    int* data;
    size_t size;
public:
    Widget& operator=(Widget other) {  // (1) copy by VALUE — if copy throws, *this is untouched
        swap(*this, other);             // (2) swap is noexcept — always succeeds
        return *this;                   // (3) old data destroyed in 'other's destructor
    }

    friend void swap(Widget& a, Widget& b) noexcept {
        using std::swap;
        swap(a.data, b.data);
        swap(a.size, b.size);
    }
};
```

## 4. `noexcept` and `std::move_if_noexcept`

`std::vector` uses `move_if_noexcept` during reallocation to maintain the strong exception guarantee:

```cpp
// During vector reallocation (push_back triggers capacity increase):

// IF move constructor is noexcept:
//   → vector MOVES elements to new buffer (fast)
//   → if any move fails... well, it can't (noexcept)

// IF move constructor MIGHT throw:
//   → vector COPIES elements to new buffer (slow but safe)
//   → if a copy throws, original buffer is still intact (strong guarantee)
//   → the partially-filled new buffer is destroyed

// Implementation sketch:
template <typename T>
void vector<T>::reallocate() {
    T* new_buf = allocate(new_capacity);
    for (size_t i = 0; i < size_; i++) {
        // move_if_noexcept returns T&& if noexcept(T(T&&)), else const T&
        new (new_buf + i) T(std::move_if_noexcept(old_buf[i]));
    }
}
```

**This is why marking move operations `noexcept` is critical for performance:**
```cpp
class Widget {
public:
    Widget(Widget&& other) noexcept;             // ← vector will MOVE (fast)
    Widget& operator=(Widget&& other) noexcept;  // ← vector will MOVE (fast)
};
```

## 5. Exception Object Lifetime

```cpp
try {
    throw std::runtime_error("error");
    // Exception object is allocated on a special exception stack (or heap)
    // It persists until the matching catch handler exits
} catch (const std::exception& e) {
    // 'e' is a reference to the exception object
    // The object is alive throughout this catch block
    throw;  // RE-THROW: same object, no copy, preserves dynamic type
    // throw e;  // BAD: creates a COPY, may SLICE if e is a derived type
}
// Exception object destroyed here (after catch block exits)
```
