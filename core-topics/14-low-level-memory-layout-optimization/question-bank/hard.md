# Low-Level Memory Layout & Optimization — Hard Questions

---

**Q1. Describe the exact memory layout of an object that contains virtual functions. What happens under multiple inheritance?**

A: When a class defines or inherits a virtual function, the compiler inserts a hidden pointer into the object, usually called the `vptr` (virtual pointer). 

- **Layout**: Compilers typically place the `vptr` at the very beginning of the object's memory layout (offset 0), followed by the member variables.
- **VTable**: The `vptr` points to the `vtable` (virtual table) for that specific class. The vtable is an array of function pointers. There is exactly one vtable per *class*.

**Multiple Inheritance**:
If `class C : public A, public B`, the memory layout of `C` actually contains **two `vptrs`**.
```
[0-7] vptr for A -> C's vtable for A
[8-11] A's members
[12-19] vptr for B -> C's vtable for B
[20-23] B's members
[24-27] C's members
```
When you cast a `C*` to a `B*`, the compiler *physically adds 12 bytes* to the pointer address so that the pointer is looking at the `vptr for B`. If `B` calls a virtual function overridden by `C`, the vtable uses a **thunk** (a tiny assembly snippet) to subtract 12 bytes from the `this` pointer before calling `C`'s method.

---

**Q2. What is Pointer Aliasing and why does it make C++ harder to optimize than Fortran?**

A: Pointer aliasing occurs when two pointers point to the same memory location.
```cpp
void add(int* a, int* b, int* result, size_t n) {
    for (size_t i = 0; i < n; i++) {
        result[i] = a[i] + b[i];
    }
}
```
The compiler wants to auto-vectorize this loop using SIMD instructions. However, in C++, it is totally legal for `result` to point to the same memory as `a`. If they overlap, writing to `result` modifies `a` mid-loop, meaning parallel SIMD execution would yield incorrect results.

Because the compiler cannot prove they *don't* overlap, it generates slow, scalar, byte-by-byte code.
**Fix**: Use the non-standard (but widely supported) `__restrict__` keyword: `int* __restrict__ result`. This promises the compiler that `result` is the *only* pointer accessing that memory block, unlocking massive vectorization optimizations.

---

**Q3. What is the true cost of a virtual function call?**

A: Calling a virtual function requires **double indirection**:
1. Dereference the object pointer to find the `vptr`.
2. Follow the `vptr` to the `vtable`.
3. Look up the function pointer at the known offset in the vtable.
4. Jump to the address of the function pointer.

While the pointer dereferencing itself is cheap, the true cost is the **inability of the CPU to predict the branch**. Because the destination address is not known at compile time, the CPU cannot pre-fetch instructions, leading to pipeline stalls. Furthermore, the compiler **cannot inline** a virtual function call.

---

**Q4. What is Devirtualization and how does the `final` keyword help?**

A: Devirtualization is a compiler optimization where a dynamic virtual dispatch is converted into a static, direct function call (which can then be inlined).

The `final` keyword (introduced in C++11) indicates that a virtual function cannot be overridden in a derived class, or that a class itself cannot be inherited from. 
If you have `Base* p = new FinalDerived()`, and call a `final` method, the compiler knows *exactly* which function will be executed. It skips the vtable lookup entirely and directly calls the function.

---

**Q5. Explain the Empty Base Optimization (EBO).**

A: In C++, the size of an empty class is 1 byte (to ensure unique memory addresses). However, if an empty class is used as a *base class*, the compiler is allowed to optimize its size to 0 bytes within the derived class.

```cpp
class Empty {};
class ContainsEmpty {
    Empty e; // Size: 1
    int x;   // Size: 4 + 3 padding
}; // Total size: 8 bytes

class InheritsEmpty : public Empty { // Empty base class
    int x; // Size: 4
}; // Total size: 4 bytes! (EBO applied)
```
This is heavily used in standard library components like `std::unique_ptr` with custom deleters. If the custom deleter is an empty struct (like a stateless lambda or functor), `std::unique_ptr` inherits from it privately, ensuring the `unique_ptr` stays exactly the size of one raw pointer (8 bytes).

---

## Gotcha Code

```cpp
// Q: What is the output?
class Base {
public:
    virtual ~Base() = default;
};

class Derived1 : virtual public Base { int a; };
class Derived2 : virtual public Base { int b; };
class Multi : public Derived1, public Derived2 {};

std::cout << sizeof(Multi);

// A: Typically 40 bytes on a 64-bit architecture!
// Virtual inheritance solves the diamond problem, but it introduces massive overhead.
// The compiler must inject multiple 'vptrs' AND virtual base table pointers (vbtbl)
// into the layout of Derived1 and Derived2 so that Multi can resolve the single instance 
// of Base at runtime. Avoid virtual inheritance in performance-critical code!
```
