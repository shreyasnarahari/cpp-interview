# Concurrency — Real Interview Questions

> No answers — attempt cold first.

## Theory Questions
1. How do you create a thread in C++? What happens if you don't join or detach?
2. What is a mutex? What is `std::lock_guard`?
3. What is a deadlock? Give an example and explain how to prevent it.
4. What is `std::condition_variable`? What are spurious wakeups?
5. What is `std::atomic`? When would you use it vs a mutex?
6. What is `std::scoped_lock` (C++17) and why is it preferred?
7. What is the difference between `std::lock_guard` and `std::unique_lock`?
8. What is a data race? How is it different from a race condition?
9. Explain `std::async` — what are the launch policies?
10. What are memory orderings? Name the four main ones.
11. What is false sharing? How do you avoid it?
12. What is `std::jthread` (C++20)?

## Code Questions

```cpp
// Q1: What's wrong?
std::mutex mtx;
void increment(int& counter) {
    mtx.lock();
    counter++;
    if (counter > 100) return;
    mtx.unlock();
}
```

```cpp
// Q2: Is this thread-safe?
bool ready = false;
void producer() { /* prepare data */ ready = true; }
void consumer() { while (!ready) {} /* use data */ }
```

```cpp
// Q3: Deadlock?
std::mutex m1, m2;
void threadA() { std::lock_guard l1(m1); std::lock_guard l2(m2); }
void threadB() { std::lock_guard l1(m2); std::lock_guard l2(m1); }
```

## Implementation Problems
1. Implement a thread-safe queue (producer-consumer)
2. Implement a thread-safe singleton (Meyer's singleton)
3. Implement a reader-writer lock using `std::shared_mutex`
4. Implement a simple thread pool
