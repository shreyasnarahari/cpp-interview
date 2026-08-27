#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>

namespace sys::ipc {

/**
 * @brief High-Performance Cross-Process Futex Mutex.
 * 
 * Uses the classic 3-state Drepper Futex algorithm:
 *   0: Unlocked
 *   1: Locked (No Waiters)
 *   2: Locked (One or more processes waiting in kernel)
 */
class FutexMutex {
public:
    constexpr FutexMutex() noexcept : state_(0) {}

    void lock() noexcept {
        uint32_t expected = 0;
        // Fast Path: Unlocked (0) -> Locked without waiters (1)
        if (state_.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
            return;
        }

        // Slow Path: Transition to contended state (2) and wait in kernel
        if (expected != 2) {
            expected = state_.exchange(2, std::memory_order_acquire);
        }

        while (expected != 0) {
            // Sleep in kernel queue until woken
            ::syscall(SYS_futex, reinterpret_cast<uint32_t*>(&state_), FUTEX_WAIT, 2, nullptr, nullptr, 0);
            expected = state_.exchange(2, std::memory_order_acquire);
        }
    }

    [[nodiscard]] bool try_lock() noexcept {
        uint32_t expected = 0;
        return state_.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
    }

    void unlock() noexcept {
        // Fast Path: If state was 1 (no waiters), decrementing leaves state 0 with ZERO syscalls!
        if (state_.fetch_sub(1, std::memory_order_release) != 1) {
            // Slow Path: There were waiters; set state to 0 and wake 1 waiting process
            state_.store(0, std::memory_order_release);
            ::syscall(SYS_futex, reinterpret_cast<uint32_t*>(&state_), FUTEX_WAKE, 1, nullptr, nullptr, 0);
        }
    }

private:
    alignas(64) std::atomic<uint32_t> state_{0};
};

class ScopedFutexLock {
public:
    explicit ScopedFutexLock(FutexMutex& mtx) noexcept : mtx_(mtx) {
        mtx_.lock();
    }

    ~ScopedFutexLock() noexcept {
        mtx_.unlock();
    }

    ScopedFutexLock(const ScopedFutexLock&) = delete;
    ScopedFutexLock& operator=(const ScopedFutexLock&) = delete;

private:
    FutexMutex& mtx_;
};

} // namespace sys::ipc
