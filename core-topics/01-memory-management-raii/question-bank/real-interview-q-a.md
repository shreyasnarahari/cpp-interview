# Memory Management & RAII — Real Interview Questions

> Questions from real C++ interviews. No answers — attempt cold first,
> then check in easy.md / medium.md / hard.md.

---

## Theory Questions

1. What is RAII? Why is it considered the most important C++ idiom?
2. What is the difference between stack and heap allocation? When would you use each?
3. What does `new` do behind the scenes? (hint: two steps)
4. What happens if you call `delete` on a nullptr? What about `delete[]` on a non-array?
5. What is `std::unique_ptr`? What operations does it support / not support?
6. What is `std::shared_ptr`? What is the overhead compared to a raw pointer?
7. What is `std::weak_ptr`? Why does it exist?
8. Why should you prefer `std::make_unique` / `std::make_shared` over `new`?
9. What is the Rule of Three? The Rule of Five? The Rule of Zero?
10. What is a custom deleter? When would you use one with `unique_ptr` or `shared_ptr`?
11. What is the control block in `shared_ptr`? What does it contain?
12. Is the reference count in `shared_ptr` thread-safe? Is accessing the pointed-to object thread-safe?
13. What is `enable_shared_from_this`? When do you need it?
14. What is placement new? When would you use it?
15. What tools can you use to detect memory leaks? (name at least 3)
16. What is the difference between `delete` and `delete[]`? What happens if you mix them?
17. What is a memory leak? What is a dangling pointer? How are they related?
18. What is stack overflow? What causes it?
19. Explain deterministic destruction. How does it differ from garbage collection?
20. What is the Pimpl idiom? How does it relate to `unique_ptr`?

---

## "What's Wrong?" Code Questions

```cpp
// Q1: What's wrong with this code?
void process(const std::string& filename) {
    FILE* f = fopen(filename.c_str(), "r");
    // ... process file ...
    if (hasError()) return;    // early return
    fclose(f);
}
```

```cpp
// Q2: What's wrong with this code?
class ResourceManager {
    int* data;
public:
    ResourceManager(int n) : data(new int[n]) {}
    ~ResourceManager() { delete data; }
};
```

```cpp
// Q3: What's wrong with this code?
std::shared_ptr<int> sp1(new int(42));
std::shared_ptr<int> sp2(sp1.get());
```

```cpp
// Q4: What's wrong with this code?
struct Node {
    std::shared_ptr<Node> next;
    std::shared_ptr<Node> prev;
};
auto a = std::make_shared<Node>();
auto b = std::make_shared<Node>();
a->next = b;
b->prev = a;
```

```cpp
// Q5: What could go wrong here?
void f(std::shared_ptr<Widget> sp, int priority) { /*...*/ }
f(std::shared_ptr<Widget>(new Widget), computePriority());
```

```cpp
// Q6: Is this valid?
auto sp = std::make_shared<int>(42);
int* raw = sp.get();
delete raw;
```

```cpp
// Q7: What's wrong?
class Widget {
    std::unique_ptr<int> data;
public:
    Widget(int val) : data(std::make_unique<int>(val)) {}
};
Widget w1(42);
Widget w2 = w1;  // ?
```

```cpp
// Q8: What's the issue?
std::vector<std::unique_ptr<Widget>> widgets;
auto w = std::make_unique<Widget>();
widgets.push_back(w);  // ?
```

```cpp
// Q9: Is this safe?
int* p = new int(42);
std::unique_ptr<int> up1(p);
std::unique_ptr<int> up2(p);
```

```cpp
// Q10: What's the problem?
class Base {
public:
    ~Base() { std::cout << "Base\n"; }  // not virtual
};
class Derived : public Base {
    std::vector<int> data;
public:
    Derived() : data(1000000) {}
};
Base* b = new Derived();
delete b;
```

---

## Implementation Problems

1. **RAII File Handle**: Implement a class `FileHandle` that wraps `FILE*` using RAII. Support move but not copy.

2. **Simple unique_ptr**: Implement a simplified `UniquePtr<T>` with constructor, destructor, move operations, `get()`, `release()`, `reset()`, and `operator*`/`operator->`.

3. **Shared pointer sketch**: Implement a simplified `SharedPtr<T>` with reference counting. Handle copy, destruction, and `use_count()`.

4. **Object pool**: Implement a simple `ObjectPool<T>` that pre-allocates N objects and hands them out via `acquire()` / `release()`. Use RAII to ensure returned objects go back to the pool.

5. **Detect the leak**: Given a code snippet with 3 interleaved memory operations, identify which allocations leak and fix all of them using RAII.

6. **Custom deleter**: Write a `unique_ptr` wrapper for a C library handle (e.g., `sqlite3*`, `FILE*`) with a custom deleter.
