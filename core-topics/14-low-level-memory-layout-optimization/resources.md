# Resources: Low-Level Memory Layout & Optimization

## Must-Read Materials
- **Agner Fog's Optimization Manuals**
  - *Why*: The absolute holy grail for low-level software optimization. Covers CPU instruction sets, microarchitecture, and C++ specific optimizations. Read *Software optimization resources*.
- **"What Every Programmer Should Know About Memory" by Ulrich Drepper**
  - *Why*: Deep dive into how CPU caches, RAM, and virtual memory actually work. Essential for understanding cache locality.

## Must-Watch Talks
- **"Data-Oriented Design and C++" by Mike Acton (CppCon 2014)**
  - *Why*: A legendary talk that shifts the mindset from Object-Oriented Programming to Data-Oriented Design. Focuses heavily on cache lines, data transformations, and treating data as the priority over abstractions.
- **"Efficiency with Algorithms, Performance with Data Structures" by Chandler Carruth (CppCon 2014)**
  - *Why*: Explains why `std::list` is almost always a terrible choice and why `std::vector` (contiguous memory) reigns supreme due to hardware prefetchers.
- **"Understanding Compiler Optimization" by Chandler Carruth**
  - *Why*: Great insights into what the compiler can (and cannot) do for you, particularly around inlining and devirtualization.

## Recommended Reading
- **Effective C++ by Scott Meyers** (Item 39: Use private inheritance judiciously - covers Empty Base Optimization)
- **Computer Systems: A Programmer's Perspective (CS:APP)**
  - Read Chapter 6 on The Memory Hierarchy.
