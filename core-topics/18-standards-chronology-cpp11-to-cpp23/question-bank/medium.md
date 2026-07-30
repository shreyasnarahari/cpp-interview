# Intermediate Questions: Modern Features & Refactoring

### Q1: Compare SFINAE (C++11) with Concepts (C++20).
```cpp
// SFINAE (C++11)
template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
void process(T val);

// Concepts (C++20)
template <std::integral T>
void process(T val);
```
Concepts provide human-readable compiler diagnostic messages and clean constraint syntax.
