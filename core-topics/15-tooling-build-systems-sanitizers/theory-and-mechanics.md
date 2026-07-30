# Theory & Mechanics: Tooling, Build Systems & Sanitizers

## 1. AddressSanitizer (ASan) — Shadow Memory Model

ASan instruments every memory access with a shadow memory check. It maps **8 bytes of application memory to 1 byte of shadow memory**:

```
Shadow byte calculation:
  shadow_addr = (mem_addr >> 3) + shadow_offset

Shadow byte values:
  0x00        = all 8 bytes are addressable (valid)
  0x01 - 0x07 = first N bytes addressable, rest poisoned
  0xFA        = stack left redzone
  0xFB        = stack mid redzone
  0xFC        = stack right redzone
  0xFD        = freed heap memory (use-after-free detection)
  0xF1        = heap left redzone (underflow detection)
  0xF3        = heap right redzone (overflow detection)

Instrumented memory access (compiler inserts before every load/store):
  shadow = *(shadow_offset + (addr >> 3));
  if (shadow != 0 && ((addr & 7) + access_size > shadow))
      __asan_report_error(addr);
```

### What ASan detects:
| Bug | How detected |
|---|---|
| Heap buffer overflow | Redzones around `malloc` allocations |
| Stack buffer overflow | Redzones around stack variables |
| Use-after-free | Quarantine zone — freed memory poisoned, not immediately reused |
| Use-after-return | Optional flag: `detect_stack_use_after_return=1` |
| Double free | Checks freed-memory shadow state |
| Memory leaks | LeakSanitizer (LSan) runs at exit, integrated with ASan |

**Overhead:** ~2× slowdown, ~3× memory usage. Cannot run with TSan simultaneously.

## 2. ThreadSanitizer (TSan) — Vector Clock Algorithm

TSan detects **data races** (concurrent unsynchronized access to shared memory where at least one access is a write):

```
TSan maintains a vector clock per thread:

Thread 1 clock: [T1:5, T2:3, T3:0]  ← "I've seen T2's events up to 3"
Thread 2 clock: [T1:2, T2:7, T3:1]

On every memory access, TSan records:
  { thread_id, clock, address, is_write }

Race detection:
  If Thread 1 writes addr X at clock [5,3,0]
  And Thread 2 reads addr X at clock [2,7,1]
  Thread 2's knowledge of T1 is 2 < 5 (T1's actual write clock)
  → T2 hasn't synchronized with T1's write → DATA RACE reported!
```

**Overhead:** ~5-15× slowdown, ~5-10× memory usage. The high overhead is why TSan is used in CI testing, not production.

### ASan vs TSan — Cannot combine:
ASan and TSan use **different shadow memory layouts** and cannot coexist. Run them in separate builds:
```bash
# Build A — memory bugs:
g++ -fsanitize=address,undefined -fno-omit-frame-pointer -g main.cpp

# Build B — data races:
g++ -fsanitize=thread -fno-omit-frame-pointer -g main.cpp
```

## 3. UndefinedBehaviorSanitizer (UBSan)

UBSan inserts lightweight runtime checks for common UB:

```
Instrumentation points:
  Signed integer overflow:  check before +, -, * on signed types
  Shift violations:         check shift amount >= 0 and < bit_width
  Null pointer dereference: check pointer before every deref
  Misaligned access:        check pointer alignment for typed access
  Signed integer truncation: check for narrowing conversions
  Unreachable code:         trap on __builtin_unreachable()
```

**Overhead:** ~10-20% — lightweight enough to enable in development builds permanently. Can combine with ASan:
```bash
g++ -fsanitize=address,undefined -fno-omit-frame-pointer -g main.cpp
```

## 4. CMake Target DAG — Modern Architecture

Modern CMake is **target-based** — dependencies form a Directed Acyclic Graph (DAG):

```cmake
# Library target — defines its own compile requirements:
add_library(networking src/tcp.cpp src/udp.cpp)
target_include_directories(networking PUBLIC include/)     # consumers inherit
target_compile_features(networking PUBLIC cxx_std_20)      # consumers inherit
target_link_libraries(networking PRIVATE ssl crypto)        # not propagated

# Executable target — dependencies propagate transitively:
add_executable(server main.cpp)
target_link_libraries(server PRIVATE networking)
# server automatically gets:
#   - networking's PUBLIC include dirs
#   - networking's PUBLIC compile features (C++20)
#   - BUT NOT ssl/crypto (they're PRIVATE to networking)
```

```
Scope propagation:
  PRIVATE:   applies only to this target
  PUBLIC:    applies to this target AND all targets linking against it
  INTERFACE: applies ONLY to targets linking against it (header-only libs)
```

## 5. Compiler Optimization Levels

```
-O0  No optimization. Fastest compile. Best debugging experience.
     Variables are in memory (not registers). No inlining.

-O1  Basic optimizations. Moderate compile time.
     Dead code elimination, basic inlining.

-O2  Recommended for production. Good balance of speed and compile time.
     Loop unrolling, vectorization, inline expansion, instruction scheduling.

-O3  Aggressive. Longer compile, larger binary, sometimes SLOWER than -O2.
     Aggressive inlining (code bloat → icache pressure).
     Auto-vectorization, loop transformations.

-Os  Optimize for SIZE. Like -O2 but avoids optimizations that increase code size.
     Better icache behavior — sometimes faster than -O3 for large codebases.

-Ofast  -O3 + -ffast-math. Breaks IEEE float compliance.
     Enables FMA, reassociation, reciprocal approximation.
     Suitable for non-financial numerical computing.
```

### Link-Time Optimization (LTO):
```bash
g++ -flto -O2 -c a.cpp -o a.o
g++ -flto -O2 -c b.cpp -o b.o
g++ -flto -O2 a.o b.o -o program
# Compiler sees ALL translation units at link time:
# → cross-file inlining
# → cross-file devirtualization
# → cross-file dead code elimination
# Typical speedup: 5-20% on large projects
```

## 6. Debugging Methodology — Core Dump Analysis

```bash
# Enable core dumps:
ulimit -c unlimited

# Compile with debug symbols:
g++ -g -O0 -fsanitize=address main.cpp -o program

# When it crashes, analyze the core dump:
gdb ./program core
(gdb) bt          # backtrace — shows call stack
(gdb) frame 3     # jump to frame 3
(gdb) print var   # inspect variable
(gdb) info locals # show all local variables
(gdb) list        # show source code around current line
```
