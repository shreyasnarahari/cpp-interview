# Module 01: OS ABI, Syscall Internals, and Binary Anatomy

# Test executable for raw syscall wrappers
add_executable(test_raw_syscalls tests/test_raw_syscalls.cpp)
target_include_directories(test_raw_syscalls PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exercises)
target_link_libraries(test_raw_syscalls PRIVATE sys_common)

# Mini-project subdirectories
add_subdirectory(projects/lightweight_elf_inspector)
