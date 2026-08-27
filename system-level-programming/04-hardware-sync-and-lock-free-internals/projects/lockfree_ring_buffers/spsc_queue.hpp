#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <array>
#include <optional>
#include <utility>
#include <new>

namespace sys::lockfree {

/**
 * @brief Ultra-Low Latency Wait-Free Single-Producer Single-Consumer (SPSC) Queue.
 * 
 * Features:
 * - Deterministic O(1) wait-free push and pop.
 * - Cacheline alignment (alignas(64)) eliminating false sharing.
 * - Cached head/tail positions to minimize inter-core cache bouncing.
 * - Fast power-of-2 bitwise masking.
 */
template <typename T, size_t Capacity = 65536>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static constexpr size_t MASK = Capacity - 1;

public:
    SPSCQueue() noexcept {
        for (size_t i = 0; i < Capacity; ++i) {
            new (&storage_[i]) Storage();
        }
    }

    ~SPSCQueue() noexcept {
        T dummy;
        while (pop(dummy)) {}
    }

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    /**
     * @brief Producer: Pushes an item into the queue.
     */
    template <typename... Args>
    [[nodiscard]] bool emplace(Args&&... args) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);

        // Check if queue is full using cached head to prevent reading atomic head_ every time
        if (tail - cached_head_ >= Capacity) {
            cached_head_ = head_.load(std::memory_order_acquire);
            if (tail - cached_head_ >= Capacity) {
                return false; // Queue is full
            }
        }

        // Construct item in pre-allocated slot
        auto* slot = reinterpret_cast<T*>(&storage_[tail & MASK]);
        new (slot) T(std::forward<Args>(args)...);

        // Publish updated tail to consumer
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool push(const T& val) noexcept {
        return emplace(val);
    }

    [[nodiscard]] bool push(T&& val) noexcept {
        return emplace(std::move(val));
    }

    /**
     * @brief Consumer: Pops an item from the queue.
     */
    [[nodiscard]] bool pop(T& out) noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);

        // Check if queue is empty using cached tail
        if (head == cached_tail_) {
            cached_tail_ = tail_.load(std::memory_order_acquire);
            if (head == cached_tail_) {
                return false; // Queue is empty
            }
        }

        auto* slot = reinterpret_cast<T*>(&storage_[head & MASK]);
        out = std::move(*slot);
        slot->~T();

        // Publish updated head to producer
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] size_t size() const noexcept {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t tail = tail_.load(std::memory_order_relaxed);
        return (tail >= head) ? (tail - head) : 0;
    }

private:
    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

    // Producer state (Core 1)
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) size_t cached_head_{0};

    // Consumer state (Core 2)
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) size_t cached_tail_{0};

    // Ring buffer backing storage
    alignas(64) Storage storage_[Capacity];
};

} // namespace sys::lockfree
