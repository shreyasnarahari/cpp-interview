# Lambda Expressions — Real Interview Questions

> No answers — attempt cold first.

## Theory Questions
1. What is a lambda expression in C++? What is the syntax?
2. Explain capture modes: [=], [&], [x], [&x], [this], [*this].
3. What is a mutable lambda? When do you need it?
4. What is the difference between a lambda and `std::function`?
5. What is a generic lambda (C++14)? How does it work internally?
6. What is an init capture (C++14)? How do you move a unique_ptr into a lambda?
7. What is the lifetime issue with lambda captures by reference?
8. What is an immediately invoked lambda expression (IILE)?
9. What is a template lambda (C++20)?
10. Can you assign one lambda to another of the same signature?

## Code Questions

```cpp
// Q1: What does this print?
int x = 10;
auto fn = [x]() mutable { x += 5; std::cout << x << " "; };
fn(); fn(); std::cout << x;
```

```cpp
// Q2: What's the problem?
std::function<void()> createTimer() {
    std::string msg = "timeout!";
    return [&msg]() { std::cout << msg; };
}
```

```cpp
// Q3: Does this compile?
auto fn = [](int a, int b) { return a + b; };
fn = [](int a, int b) { return a * b; };
```

```cpp
// Q4: What happens?
std::vector<std::function<void()>> fns;
for (int i = 0; i < 5; i++) {
    fns.push_back([&i]() { std::cout << i << " "; });
}
for (auto& f : fns) f();
```

## Implementation Problems
1. Write a memoize wrapper using lambdas and unordered_map
2. Implement a simple callback system using std::function
3. Write a compose function: compose(f, g)(x) = f(g(x))
