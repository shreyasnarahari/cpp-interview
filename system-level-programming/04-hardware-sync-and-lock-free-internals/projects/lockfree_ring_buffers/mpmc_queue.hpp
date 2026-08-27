#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>
#include <utility>
#include <new>

namespace sys::lockfree {

/**
 * @brief Bounded Lock-Free Multi-Producer Multi-Consumer (MPMC) Queue.
 * 
 * Implemented using Dmitry Vyukov's array-based algorithm.
 * Provides thread-safe, non-blocking concurrent enqueue and dequeue
 * across any number of producer and consumer threads.
 */
template <typename T>
class MPMCQueue {
public:
    explicit MPMCQueue(size_t capacity = 65536)
        : capacity_(round_up_pow2(capacity)),
          mask_(capacity_ - 1),
          buffer_(new Cell[capacity_]) {
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].sequence.store(i, std::memory_order_relaxed);
        }
        enqueue_pos_.store(0, std::memory_order_relaxed);
        dequeue_pos_.store(0, std::memory_order_relaxed);
    }

    ~MPMCQueue() {
        delete[] buffer_;
    }

    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;
    MPMCQueue(MPMCQueue&&) = delete;
    MPMCQueue& operator=(MPMCQueue&&) = delete;

    /**
     * @brief Thread-safe push operation for multiple producers.
     */
    template <typename... Args>
    [[nodiscard]] bool emplace(Args&&... args) noexcept {
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);

        for (;;) {
            Cell& cell = buffer_[pos & mask_];
            const size_t seq = cell.sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (diff == 0) {
                // Cell is ready for writing; attempt to claim this position
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    cell.data = T(std::forward<Args>(args)...);
                    // Mark cell ready for consumer (seq = pos + 1)
                    cell.sequence.store(pos + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                // Queue is full
                return false;
            } else {
                // Another thread claimed this slot; reload enqueue position
                pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
        }
    }

    [[nodiscard]] bool push(const T& data) noexcept {
        return emplace(data);
    }

    [[nodiscard]] bool push(T&& data) noexcept {
        return emplace(std::move(data));
    }

    /**
     * @brief Thread-safe pop operation for multiple consumers.
     */
    [[nodiscard]] bool pop(T& out) noexcept {
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);

        for (;;) {
            Cell& cell = buffer_[pos & mask_];
            const size_t seq = cell.sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (diff == 0) {
                // Cell has data; attempt to claim this slot
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    out = std::move(cell.data);
                    // Mark cell ready for next wrap-around producer (seq = pos + capacity_)
                    cell.sequence.store(pos + mask_ + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                // Queue is empty
                return false;
            } else {
                // Another consumer claimed this slot; reload dequeue position
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_;
    }

private:
    struct alignas(64) Cell {
        std::atomic<size_t> sequence{0};
        T data{};
    };

    static size_t round_up_pow2(size_t v) noexcept {
        if (v < 2) return 2;
        --v;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v |= v >> 32;
        return v + 1;
    }

    const size_t capacity_;
    const size_t mask_;
    Cell* const buffer_;

    alignas(64) std::atomic<size_t> enqueue_pos_{0};
    alignas(64) std::atomic<size_t> dequeue_pos_{0};
};

} // namespace sys::lockfree
