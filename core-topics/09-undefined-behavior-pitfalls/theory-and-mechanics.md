# Theory & Mechanics: Undefined Behavior & Pitfalls

## 1. Why UB Exists — The Optimization Contract

Undefined behavior is not a bug in the standard — it's a deliberate **contract between the programmer and the compiler**. By declaring certain operations undefined, the standard allows the compiler to **assume they never happen** and optimize accordingly:

```cpp
int f(int x) {
    if (x + 1 > x)    // always true for valid (non-overflowing) ints
        return 1;
    return 0;
}
// The compiler assumes signed overflow never happens (it's UB)
// Therefore x + 1 > x is ALWAYS true
// Compiler optimizes to: return 1;  (the else branch is dead code)
```

This is called **"time travel" optimization** — the compiler can reason backward from UB: "If this branch would cause UB, the branch is never taken, so I can eliminate it."

## 2. Strict Aliasing Rule

The compiler assumes that **pointers of unrelated types do not alias** (point to the same memory). This enables Type-Based Alias Analysis (TBAA), a powerful optimization:

```cpp
void update(int* ip, float* fp) {
    *ip = 10;
    *fp = 3.14f;
    printf("%d", *ip);  // Compiler assumes *ip is still 10
    // Because int* and float* can't alias (strict aliasing rule)
    // So it optimizes to: printf("%d", 10);  — no reload from memory
}

// VIOLATION — UB:
int x = 42;
float* fp = reinterpret_cast<float*>(&x);
*fp = 3.14f;   // UB — writing float through pointer obtained from int

// EXCEPTION — char/byte types can alias anything:
int x = 42;
char* cp = reinterpret_cast<char*>(&x);
cp[0] = 0;    // LEGAL — char* can alias any type
```

### Type punning done safely:
```cpp
// C++20 — std::bit_cast (safe, constexpr):
float f = 3.14f;
uint32_t bits = std::bit_cast<uint32_t>(f);  // reinterprets bits, no UB

// Pre-C++20 — memcpy (safe, optimizer sees through it):
uint32_t bits;
std::memcpy(&bits, &f, sizeof(bits));  // always safe, zero overhead at -O2
```

## 3. Signed Integer Overflow

```cpp
int x = INT_MAX;  // 2147483647
x = x + 1;        // UB!

// What the compiler does with this knowledge:
for (int i = 0; i < n; i++) {
    arr[i * 2] = 0;   // compiler assumes i * 2 never overflows
    // → can use 64-bit multiplication without overflow check
    // → can vectorize the loop
}
// If i * 2 COULD overflow, these optimizations would be invalid
```

**Unsigned integers DO wrap around** — this is defined behavior:
```cpp
unsigned int u = UINT_MAX;  // 4294967295
u = u + 1;                  // defined: u == 0 (wraps around)
```

## 4. Order of Evaluation Traps

```cpp
// Before C++17 — order of function argument evaluation is UNSPECIFIED:
f(i++, i++);  // UB — two unsequenced modifications of i

// C++17 fixes some cases:
// Chained << is left-to-right:
std::cout << i++ << i++;  // C++17: defined (left-to-right)

// But function arguments are STILL unspecified in C++17:
f(a(), b());  // a() may run before or after b() — implementation decides
```

## 5. Dangling References and Lifetime

```cpp
// Dangling pointer — returning address of local:
int* danger() {
    int x = 42;
    return &x;    // x dies → returned pointer is dangling
}

// Dangling reference — subtle version:
const std::string& danger2() {
    std::string s = "temp";
    return s;     // s dies → reference dangles
}

// Dangling string_view:
std::string_view danger3() {
    std::string s = "temp";
    return s;     // string_view holds pointer to s's buffer
}                 // s dies → string_view's pointer is dangling

// SAFE — const ref extends temporary lifetime:
const std::string& safe = std::string("temp");  // lifetime extended
// But ONLY for local const refs, NOT function return values!
```

## 6. Sanitizer Mechanics

### AddressSanitizer (ASan) — Shadow Memory:
ASan maps every 8 bytes of application memory to 1 byte of "shadow memory":
```
Shadow byte values:
  0x00 = all 8 bytes addressable
  0x01-0x07 = first N bytes addressable
  0xFA = stack redzone
  0xFD = freed memory
  0xF1 = stack left redzone

On every memory access:
  shadow_addr = (addr >> 3) + shadow_offset
  if (*shadow_addr != 0) → report error!
```
Overhead: ~2× slowdown, ~3× memory usage.

### UndefinedBehaviorSanitizer (UBSan):
Instruments specific operations with runtime checks:
```
Signed overflow: adds overflow check before every +, -, *
Null deref: checks pointer before every dereference
Shift amount: validates shift count < bit-width
Alignment: verifies pointer alignment before typed access
```
Overhead: ~10-20% slowdown — lightweight enough for production testing.
