#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/mman.h>
#include <unistd.h>
#include <new>
#include <utility>
#include <mutex>
#include <vector>
#include <iostream>
#include <cassert>

namespace sys::alloc {

/**
 * @brief High-Performance Fixed-Size Slab Allocator.
 * 
 * Uses an intrusive free-list embedded directly in unallocated chunks,
 * achieving O(1) allocation and deallocation with zero external heap overhead.
 */
template <typename T, size_t ObjectsPerSlab = 1024>
class SlabAllocator {
public:
    static constexpr size_t OBJECT_SIZE = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
    static constexpr size_t ALIGNMENT = alignof(T) < alignof(void*) ? alignof(void*) : alignof(T);
    static constexpr size_t CHUNK_SIZE = (OBJECT_SIZE + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
    static constexpr size_t SLAB_SIZE = CHUNK_SIZE * ObjectsPerSlab;

    SlabAllocator() {
        grow_slab();
    }

    ~SlabAllocator() {
        for (void* slab : slabs_) {
            ::munmap(slab, SLAB_SIZE);
        }
    }

    SlabAllocator(const SlabAllocator&) = delete;
    SlabAllocator& operator=(const SlabAllocator&) = delete;
    SlabAllocator(SlabAllocator&&) = delete;
    SlabAllocator& operator=(SlabAllocator&&) = delete;

    /**
     * @brief O(1) raw chunk allocation.
     */
    [[nodiscard]] void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!free_head_) {
            grow_slab();
        }

        if (!free_head_) {
            return nullptr;
        }

        FreeNode* node = free_head_;
        free_head_ = node->next;
        ++active_objects_;
        return static_cast<void*>(node);
    }

    /**
     * @brief O(1) raw chunk deallocation.
     */
    void deallocate(void* ptr) noexcept {
        if (!ptr) return;

        std::lock_guard<std::mutex> lock(mutex_);
        auto* node = static_cast<FreeNode*>(ptr);
        node->next = free_head_;
        free_head_ = node;
        --active_objects_;
    }

    /**
     * @brief Type-safe object construction in pre-allocated chunk.
     */
    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) {
        void* mem = allocate();
        if (!mem) return nullptr;
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    /**
     * @brief Type-safe object destruction and chunk release.
     */
    void destroy(T* ptr) noexcept {
        if (!ptr) return;
        ptr->~T();
        deallocate(ptr);
    }

    [[nodiscard]] size_t get_active_objects() const noexcept {
        return active_objects_;
    }

    [[nodiscard]] size_t get_total_capacity() const noexcept {
        return slabs_.size() * ObjectsPerSlab;
    }

    [[nodiscard]] size_t get_slab_count() const noexcept {
        return slabs_.size();
    }

private:
    struct FreeNode {
        FreeNode* next{nullptr};
    };

    void grow_slab() {
        void* slab_mem = ::mmap(
            nullptr,
            SLAB_SIZE,
            PROT_READ | PROT_WRITE,
            MAP_ANONYMOUS | MAP_PRIVATE,
            -1,
            0
        );

        if (slab_mem == MAP_FAILED) {
            return;
        }

        slabs_.push_back(slab_mem);

        // Chain all chunks in the new slab into the intrusive free-list
        auto* base = static_cast<uint8_t*>(slab_mem);
        for (size_t i = 0; i < ObjectsPerSlab; ++i) {
            auto* node = reinterpret_cast<FreeNode*>(base + (i * CHUNK_SIZE));
            node->next = free_head_;
            free_head_ = node;
        }
    }

    FreeNode* free_head_{nullptr};
    std::vector<void*> slabs_;
    size_t active_objects_{0};
    std::mutex mutex_;
};

} // namespace sys::alloc
