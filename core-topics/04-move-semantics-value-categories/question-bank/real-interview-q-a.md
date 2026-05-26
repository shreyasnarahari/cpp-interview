# Move Semantics & Value Categories — Real Interview Questions

> No answers — attempt cold first.

## Theory Questions
1. What is an lvalue? What is an rvalue? Give examples.
2. What does `std::move` actually do? Does it move anything?
3. What is a move constructor? When is it called instead of a copy constructor?
4. What is the state of a moved-from object?
5. When is the move constructor implicitly generated? When is it deleted?
6. What is copy elision? What is the difference between RVO and NRVO?
7. Why should move operations be marked `noexcept`?
8. What is a forwarding reference (universal reference)? How is it different from an rvalue reference?
9. What is perfect forwarding? Why do we need `std::forward`?
10. Explain the reference collapsing rules.
11. What are the C++17 guaranteed copy elision rules?
12. When does using `std::move` on a return value actually hurt performance?
13. What is the difference between `T&&` in a template context vs non-template context?
14. What is the value category of `std::move(x)`?

## Code Questions

```cpp
// Q1: Copy or move?
void process(std::string&& s) {
    std::string local = s;  // ?
}
```

```cpp
// Q2: How many constructions?
Widget createWidget() {
    Widget w;
    return w;
}
Widget w = createWidget();
```

```cpp
// Q3: Is this correct?
std::unique_ptr<Widget> create() {
    auto w = std::make_unique<Widget>();
    return std::move(w);
}
```

```cpp
// Q4: What's the output?
struct Loud {
    Loud() { std::cout << "ctor "; }
    Loud(const Loud&) { std::cout << "copy "; }
    Loud(Loud&&) noexcept { std::cout << "move "; }
};
Loud a;
Loud b = std::move(a);
Loud c = b;
```

## Implementation Problems
1. Implement a move-only `Buffer` class with move constructor and move assignment
2. Implement `std::forward` from scratch
3. Write a factory function using perfect forwarding
4. Show a case where `noexcept` on move changes `std::vector` behavior
