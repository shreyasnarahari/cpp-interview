# Foundational Questions: C++ Standards Evolution

### Q1: What is structured binding in C++17?
**Answer:** Structured binding allows decomposing pairs, tuples, structs, or arrays into named variables (`auto [k, v] = map.insert(...);`).

### Q2: What is `if constexpr`?
**Answer:** `if constexpr` (C++17) evaluates a condition at compile time and discards the unchosen branch, avoiding template instantiation errors on discarded branches.
