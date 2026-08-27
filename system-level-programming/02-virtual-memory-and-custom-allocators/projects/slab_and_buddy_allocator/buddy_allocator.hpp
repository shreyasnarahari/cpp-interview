#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <mutex>

namespace sys::alloc {

/**
 * @brief Binary Buddy Allocator backed by raw mmap.
 * 
 * Manages power-of-two blocks from MIN_BLOCK_SIZE (4KB, Order 0)
 * to MAX_BLOCK_SIZE (Order MAX_ORDER).
 */
class BuddyAllocator {
public:
    static constexpr size_t MIN_BLOCK_SIZE = 4096; // 4 KB (Order 0)
    static constexpr size_t MAX_ORDER = 8;         // Order 8 -> 4KB * 256 = 1 MB
    static constexpr size_t POOL_SIZE = MIN_BLOCK_SIZE << MAX_ORDER; // 1 MB

    BuddyAllocator() {
        // Allocate raw backing arena using anonymous mmap
        raw_memory_ = static_cast<uint8_t*>(::mmap(
            nullptr,
            POOL_SIZE,
            PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_PRIVATE,
            -1,
            0
        ));

        if (raw_memory_ == MAP_FAILED) {
            throw std::bad_alloc();
        }

        // Initialize top-level free block of Order MAX_ORDER
        free_lists_[MAX_ORDER].push_back(raw_memory_);
    }

    ~BuddyAllocator() {
        if (raw_memory_ && raw_memory_ != MAP_FAILED) {
            ::munmap(raw_memory_, POOL_SIZE);
        }
    }

    BuddyAllocator(const BuddyAllocator&) = delete;
    BuddyAllocator& operator=(const BuddyAllocator&) = delete;
    BuddyAllocator(BuddyAllocator&&) = delete;
    BuddyAllocator& operator=(BuddyAllocator&&) = delete;

    /**
     * @brief Allocates a block of memory of at least `bytes` size.
     */
    [[nodiscard]] void* allocate(size_t bytes) {
        if (bytes == 0 || bytes > POOL_SIZE) {
            return nullptr;
        }

        const size_t order = size_to_order(bytes);
        if (order > MAX_ORDER) {
            return nullptr;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // Find the smallest available order >= requested order
        size_t current_order = order;
        while (current_order <= MAX_ORDER && free_lists_[current_order].empty()) {
            ++current_order;
        }

        if (current_order > MAX_ORDER) {
            // Out of memory
            return nullptr;
        }

        // Pop a block from the found order
        uint8_t* block = free_lists_[current_order].back();
        free_lists_[current_order].pop_back();

        // Recursively split the block down to the requested order
        while (current_order > order) {
            --current_order;
            const size_t block_size = order_to_size(current_order);
            uint8_t* buddy = block + block_size;
            free_lists_[current_order].push_back(buddy);
            ++split_count_;
        }

        allocated_bytes_ += order_to_size(order);
        return block;
    }

    /**
     * @brief Frees a previously allocated block of the specified size.
     */
    void deallocate(void* ptr, size_t bytes) noexcept {
        if (!ptr || bytes == 0) return;

        auto* block = static_cast<uint8_t*>(ptr);
        size_t order = size_to_order(bytes);
        if (order > MAX_ORDER) return;

        std::lock_guard<std::mutex> lock(mutex_);
        allocated_bytes_ -= order_to_size(order);

        // Recursively coalesce / merge buddy blocks if available
        while (order < MAX_ORDER) {
            const size_t block_size = order_to_size(order);
            const uintptr_t offset = static_cast<uintptr_t>(block - raw_memory_);
            const uintptr_t buddy_offset = offset ^ block_size;
            uint8_t* buddy = raw_memory_ + buddy_offset;

            // Check if buddy is currently in the free list for this order
            auto& fl = free_lists_[order];
            auto it = std::find(fl.begin(), fl.end(), buddy);

            if (it == fl.end()) {
                // Buddy is not free, stop merging
                break;
            }

            // Remove buddy from free list and merge
            fl.erase(it);
            block = (offset < buddy_offset) ? block : buddy;
            ++order;
            ++merge_count_;
        }

        free_lists_[order].push_back(block);
    }

    [[nodiscard]] size_t get_allocated_bytes() const noexcept {
        return allocated_bytes_;
    }

    [[nodiscard]] size_t get_total_pool_size() const noexcept {
        return POOL_SIZE;
    }

    [[nodiscard]] uint64_t get_split_count() const noexcept {
        return split_count_;
    }

    [[nodiscard]] uint64_t get_merge_count() const noexcept {
        return merge_count_;
    }

    [[nodiscard]] static constexpr size_t order_to_size(size_t order) noexcept {
        return MIN_BLOCK_SIZE << order;
    }

    [[nodiscard]] static size_t size_to_order(size_t bytes) noexcept {
        if (bytes <= MIN_BLOCK_SIZE) return 0;
        size_t size = MIN_BLOCK_SIZE;
        size_t order = 0;
        while (size < bytes && order < MAX_ORDER) {
            size <<= 1;
            ++order;
        }
        return order;
    }

private:
    uint8_t* raw_memory_{nullptr};
    std::vector<uint8_t*> free_lists_[MAX_ORDER + 1];
    size_t allocated_bytes_{0};
    uint64_t split_count_{0};
    uint64_t merge_count_{0};
    std::mutex mutex_;
};

} // namespace sys::alloc
