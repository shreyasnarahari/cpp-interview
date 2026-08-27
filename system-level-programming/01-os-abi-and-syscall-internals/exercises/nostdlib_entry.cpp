#include "raw_syscalls.hpp"

/**
 * @brief Standalone Linux x86_64 Executable with ZERO libc / stdlib dependencies.
 * 
 * Compile with:
 *   g++ -std=c++20 -O2 -nostdlib -static nostdlib_entry.cpp -o nostdlib_app
 */

extern "C" {

__attribute__((force_align_arg_pointer))
[[noreturn]] void _start() noexcept {
    static const char msg[] = ">>> [NO-STDLIB SUCCESS]: Bare-metal user-space C++20 executing via raw inline assembly syscalls! <<<\n";
    constexpr size_t len = sizeof(msg) - 1;

    // Direct sys_write to STDOUT (fd = 1)
    sys::abi::sys_write(1, msg, len);

    // Direct sys_exit with status 0
    sys::abi::sys_exit(0);
}

}
