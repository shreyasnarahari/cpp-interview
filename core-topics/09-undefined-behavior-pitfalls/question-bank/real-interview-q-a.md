# Undefined Behavior & Common Pitfalls — Real Interview Questions

> No answers — attempt cold first, then check easy.md / medium.md / hard.md.

---

## Theory Questions

1. What is undefined behavior? Why does C++ have it?
2. Name at least 7 common sources of undefined behavior in C++.
3. What is the difference between undefined behavior, unspecified behavior, and implementation-defined behavior? Give an example of each.
4. Is signed integer overflow defined? What about unsigned?
5. What tools detect UB at runtime? Name at least 3 sanitizers.
6. Can the compiler assume that UB never happens? What are the consequences?
7. What is strict aliasing? Give an example of a violation.
8. What is a dangling reference? A dangling pointer? How do they occur?
9. Is accessing an array out of bounds UB? What about `std::vector::operator[]` vs `std::vector::at()`?
10. What is a data race? Why is it UB in C++?
11. What is the "static initialization order fiasco"? How do you fix it?
12. Is it UB to read from a `union` member that wasn't the last one written?
13. What is the order of evaluation for function arguments? Is `f(i++, i++)` well-defined?
14. Can `nullptr` dereference be "harmless"? (no — always UB, even if it "works")
15. What is the One Definition Rule (ODR)? What happens if you violate it?

---

## "What does this print?" Code Questions

```cpp
// Q1:
int i = 0;
std::cout << i++ << " " << i++ << " " << i++;
```

```cpp
// Q2:
int arr[5] = {1, 2, 3, 4, 5};
std::cout << arr[5];
```

```cpp
// Q3:
int* dangling() {
    int x = 42;
    return &x;
}
std::cout << *dangling();
```

```cpp
// Q4:
int x = INT_MAX;
x = x + 1;
std::cout << x;
```

```cpp
// Q5:
unsigned int u = 0;
u = u - 1;
std::cout << u;
```

```cpp
// Q6:
std::string& getRef() {
    std::string s = "hello";
    return s;
}
std::cout << getRef();
```

```cpp
// Q7:
int a = 1;
int b = a + (a = 2);
std::cout << b;
```

```cpp
// Q8:
class Base {
public:
    ~Base() { std::cout << "~Base "; }
};
class Derived : public Base {
    std::vector<int> v{1,2,3,4,5};
public:
    ~Derived() { std::cout << "~Derived "; }
};
Base* p = new Derived();
delete p;
```

```cpp
// Q9:
bool isEven(int x) {
    return x % 2 == 0;
}
isEven(-3);  // true or false?
```

```cpp
// Q10:
char* s = "hello";  // string literal
s[0] = 'H';         // what happens?
```

---

## Implementation Problems

1. Write a function that detects signed integer overflow before it happens.
2. Fix a code snippet with 5 different UB bugs (provided in hard.md).
3. Write a safe wrapper for array access that throws on out-of-bounds.
4. Demonstrate the "static initialization order fiasco" and fix it with a function-local static.
