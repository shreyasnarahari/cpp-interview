# Resources: Tooling, Build Systems & Sanitizers

## Must-Read Materials
- **[Modern CMake](https://cliutils.gitlab.io/modern-cmake/)**
  - *Why*: The definitive open-source guide to writing clean, target-based CMake code. Essential for understanding how to structure modern C++ projects.
- **[Clang AddressSanitizer Documentation](https://clang.llvm.org/docs/AddressSanitizer.html)**
  - *Why*: Explains the underlying mechanics (shadow memory) of how ASan catches memory bugs with minimal overhead.
- **[Clang ThreadSanitizer Documentation](https://clang.llvm.org/docs/ThreadSanitizer.html)**
  - *Why*: Crucial for understanding how data races are detected at runtime.

## Must-Watch Talks
- **"Effective CMake" by Daniel Pfeifer (CppCon 2017)**
  - *Why*: The talk that revolutionized how C++ developers write CMake, shifting the industry from global variables to target-centric configurations.
- **"Finding Bugs with Sanitizers" by Kostya Serebryany**
  - *Why*: A talk by the original authors of ASan and TSan explaining how they work under the hood and why every project should run them in CI.

## Recommended Tools to Install & Practice
- **`ccache`**: A compiler cache that dramatically speeds up recompilation by caching previous compilations and detecting when the same compilation is being done again.
- **`gdb` / `lldb`**: Learn the basic commands: `run`, `break`, `continue`, `next`, `step`, `backtrace` (`bt`), `frame`, and `print`.
- **`valgrind`**: Specifically the Memcheck tool. Understand why it's slower than ASan but doesn't require recompilation.
