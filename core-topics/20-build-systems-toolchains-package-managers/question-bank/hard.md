# Advanced Questions: LTO, Sanitizers & Compiler Passes

### Q1: How does Link-Time Optimization (`-flto`) differ from standard `-O3` compilation?
**Answer:** Standard `-O3` optimizes within individual Translation Units (TUs) and emits binary object files (`.o`). `-flto` emits GIMPLE/LLVM intermediate representation (IR) into the object files, allowing the linker to perform global interprocedural analysis, cross-TU inlining, dead code elimination, and devirtualization across all codebase source files.
