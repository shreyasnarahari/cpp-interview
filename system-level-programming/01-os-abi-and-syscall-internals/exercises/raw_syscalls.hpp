#pragma once

#include <cstdint>
#include <cstddef>

namespace sys::abi {

/**
 * @brief Canonical Linux x86_64 Syscall Numbers.
 */
namespace syscall_nr {
    inline constexpr int64_t READ    = 0;
    inline constexpr int64_t WRITE   = 1;
    inline constexpr int64_t OPEN    = 2;
    inline constexpr int64_t CLOSE   = 3;
    inline constexpr int64_t MMAP    = 9;
    inline constexpr int64_t MUNMAP  = 11;
    inline constexpr int64_t GETPID  = 39;
    inline constexpr int64_t EXIT    = 60;
}

/**
 * @brief Raw x86_64 Inline Assembly Syscall Invokers.
 * 
 * In Linux x86_64:
 * Syscall number -> %rax
 * Arg 1          -> %rdi
 * Arg 2          -> %rsi
 * Arg 3          -> %rdx
 * Arg 4          -> %r10 (Note: %r10 instead of %rcx!)
 * Arg 5          -> %r8
 * Arg 6          -> %r9
 * 
 * Hardware registers %rcx and %r11 are clobbered by the `syscall` instruction.
 */
class RawSyscall {
public:
    static inline int64_t syscall0(int64_t nr) noexcept {
        int64_t ret;
        __asm__ __volatile__(
            "syscall"
            : "=a"(ret)
            : "a"(nr)
            : "rcx", "r11", "memory"
        );
        return ret;
    }

    static inline int64_t syscall1(int64_t nr, int64_t arg1) noexcept {
        int64_t ret;
        __asm__ __volatile__(
            "syscall"
            : "=a"(ret)
            : "a"(nr), "D"(arg1)
            : "rcx", "r11", "memory"
        );
        return ret;
    }

    static inline int64_t syscall2(int64_t nr, int64_t arg1, int64_t arg2) noexcept {
        int64_t ret;
        __asm__ __volatile__(
            "syscall"
            : "=a"(ret)
            : "a"(nr), "D"(arg1), "S"(arg2)
            : "rcx", "r11", "memory"
        );
        return ret;
    }

    static inline int64_t syscall3(int64_t nr, int64_t arg1, int64_t arg2, int64_t arg3) noexcept {
        int64_t ret;
        __asm__ __volatile__(
            "syscall"
            : "=a"(ret)
            : "a"(nr), "D"(arg1), "S"(arg2), "d"(arg3)
            : "rcx", "r11", "memory"
        );
        return ret;
    }

    static inline int64_t syscall6(int64_t nr, int64_t a1, int64_t a2, int64_t a3, int64_t a4, int64_t a5, int64_t a6) noexcept {
        int64_t ret;
        register int64_t r10 __asm__("r10") = a4;
        register int64_t r8  __asm__("r8")  = a5;
        register int64_t r9  __asm__("r9")  = a6;

        __asm__ __volatile__(
            "syscall"
            : "=a"(ret)
            : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
            : "rcx", "r11", "memory"
        );
        return ret;
    }
};

/**
 * @brief Strongly-typed C++ wrappers invoking raw Linux system calls directly.
 */
inline int64_t sys_write(int fd, const void* buf, size_t count) noexcept {
    return RawSyscall::syscall3(
        syscall_nr::WRITE,
        static_cast<int64_t>(fd),
        reinterpret_cast<int64_t>(buf),
        static_cast<int64_t>(count)
    );
}

inline int64_t sys_read(int fd, void* buf, size_t count) noexcept {
    return RawSyscall::syscall3(
        syscall_nr::READ,
        static_cast<int64_t>(fd),
        reinterpret_cast<int64_t>(buf),
        static_cast<int64_t>(count)
    );
}

inline int64_t sys_open(const char* pathname, int flags, int mode = 0) noexcept {
    return RawSyscall::syscall3(
        syscall_nr::OPEN,
        reinterpret_cast<int64_t>(pathname),
        static_cast<int64_t>(flags),
        static_cast<int64_t>(mode)
    );
}

inline int64_t sys_close(int fd) noexcept {
    return RawSyscall::syscall1(
        syscall_nr::CLOSE,
        static_cast<int64_t>(fd)
    );
}

inline int64_t sys_getpid() noexcept {
    return RawSyscall::syscall0(syscall_nr::GETPID);
}

inline void* sys_mmap(void* addr, size_t length, int prot, int flags, int fd, int64_t offset) noexcept {
    int64_t res = RawSyscall::syscall6(
        syscall_nr::MMAP,
        reinterpret_cast<int64_t>(addr),
        static_cast<int64_t>(length),
        static_cast<int64_t>(prot),
        static_cast<int64_t>(flags),
        static_cast<int64_t>(fd),
        offset
    );
    return reinterpret_cast<void*>(res);
}

inline int64_t sys_munmap(void* addr, size_t length) noexcept {
    return RawSyscall::syscall2(
        syscall_nr::MUNMAP,
        reinterpret_cast<int64_t>(addr),
        static_cast<int64_t>(length)
    );
}

[[noreturn]] inline void sys_exit(int status) noexcept {
    RawSyscall::syscall1(syscall_nr::EXIT, static_cast<int64_t>(status));
    __builtin_unreachable();
}

} // namespace sys::abi
