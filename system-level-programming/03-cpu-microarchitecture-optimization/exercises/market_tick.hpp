#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <new>
#include <cstdlib>

namespace sys::simd {

/**
 * @brief Custom 32-Byte Aligned Allocator for AVX2 Compatibility.
 */
template <typename T, size_t Alignment = 32>
struct AlignedAllocator {
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using is_always_equal = std::true_type;

    template <typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };

    AlignedAllocator() noexcept = default;
    template <typename U>
    constexpr AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    [[nodiscard]] T* allocate(size_t n) {
        if (n == 0) return nullptr;
        void* ptr = nullptr;
        if (::posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, size_t) noexcept {
        ::free(p);
    }

    template <typename U>
    bool operator==(const AlignedAllocator<U, Alignment>&) const noexcept { return true; }
    template <typename U>
    bool operator!=(const AlignedAllocator<U, Alignment>&) const noexcept { return false; }
};

template <typename T>
using AlignedVector = std::vector<T, AlignedAllocator<T, 32>>;

/**
 * @brief Array-of-Structures (AoS) Tick Representation (32 Bytes).
 */
struct alignas(32) AosMarketTick {
    double price;        // 8 bytes
    double volume;       // 8 bytes
    uint64_t timestamp;  // 8 bytes
    uint32_t flags;      // 4 bytes
    char symbol[4];      // 4 bytes
};

/**
 * @brief Structure-of-Arrays (SoA) Tick Representation.
 */
struct SoaTickDataset {
    AlignedVector<double> prices;
    AlignedVector<double> volumes;
    AlignedVector<uint64_t> timestamps;
    AlignedVector<uint32_t> flags;

    explicit SoaTickDataset(size_t size)
        : prices(size), volumes(size), timestamps(size), flags(size) {}

    [[nodiscard]] size_t size() const noexcept {
        return prices.size();
    }
};

} // namespace sys::simd
