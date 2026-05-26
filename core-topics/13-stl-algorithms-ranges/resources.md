# Resources: STL Algorithms & Ranges

## Official Documentation
- [cppreference - Algorithms library](https://en.cppreference.com/w/cpp/algorithm)
- [cppreference - Ranges library (C++20)](https://en.cppreference.com/w/cpp/ranges)

## Must-Watch Talks
- **"C++ Seasoning" by Sean Parent (GoingNative 2013)**
  - *Why*: The origin of the "No Raw Loops" movement in C++. A masterclass on using `std::rotate`, `std::partition`, and `std::stable_partition` to solve complex problems elegantly.
- **"10 Core Guidelines You Need to Start Using Now" by Bjarne Stroustrup**
  - *Why*: Reinforces the idea that raw loops are error-prone and algorithms express intent.
- **"A Unifying Abstraction for C++" by Eric Niebler**
  - *Why*: The creator of range-v3 explains the philosophy behind C++20 Ranges.

## Recommended Reading
- **Effective STL by Scott Meyers**
  - Read Item 32: *Follow remove-like algorithms by erase.*
  - Read Item 43: *Prefer algorithm calls to hand-written loops.*
- **A Tutorial on C++20 Ranges** (Various blogs like Sy Brand's or Fluent C++)
  - Focus on understanding `std::views`, lazy evaluation, and the pipeline `|` operator.
