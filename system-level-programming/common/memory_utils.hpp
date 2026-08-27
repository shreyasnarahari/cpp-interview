#pragma once

#include <cstdint>
#include <cstddef>
#include <new>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#include <x86intrin.h>
#endif

namespace sys::mem {

/**
 * @brief Canonical Hardware Cache Line Sizes.
 * 
 * GCC/Clang standard library implementations can emit warnings when using
 * std::hardware_destructive_interference_size in C++20. We provide safe,
 * verified constants for x86_64 architecture.
 */
#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_destructive_interference_size;
    using std::hardware_constructive_interference_size;
#else
    inline constexpr size_t hardware_destructive_interference_size = 64;
    inline constexpr size_t hardware_constructive_interference_size = 64;
#endif

inline constexpr size_t CACHE_LINE_SIZE = 64;
inline constexpr size_t PAGE_SIZE_4KB   = 4096;
inline constexpr size_t HUGEPAGE_2MB    = 2 * 1024 * 1024;
inline constexpr size_t HUGEPAGE_1GB    = 1024 * 1024 * 1024;

/**
 * @brief Checks if a pointer or address is aligned to a specified boundary.
 */
[[nodiscard]] constexpr bool is_aligned(const void* ptr, size_t alignment) noexcept {
    return (reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)) == 0;
}

[[nodiscard]] constexpr uintptr_t align_up(uintptr_t addr, size_t alignment) noexcept {
    return (addr + (alignment - 1)) & ~(alignment - 1);
}

[[nodiscard]] inline void* align_up_ptr(void* ptr, size_t alignment) noexcept {
    return reinterpret_cast<void*>(align_up(reinterpret_cast<uintptr_t>(ptr), alignment));
}

/**
 * @brief Wrapper to ensure an object is explicitly aligned to a cache-line boundary.
 */
template <typename T>
struct alignas(CACHE_LINE_SIZE) CacheAligned {
    T value{};

    constexpr CacheAligned() = default;
    constexpr explicit CacheAligned(T val) : value(std::move(val)) {}

    constexpr T& operator*() noexcept { return value; }
    constexpr const T& operator*() const noexcept { return value; }
    constexpr T* operator->() noexcept { return &value; }
    constexpr const T* operator->() const noexcept { return &value; }

    constexpr operator T&() noexcept { return value; }
    constexpr operator const T&() const noexcept { return value; }
};

/**
 * @brief Pads an object to prevent false sharing across adjacent cache lines.
 */
template <typename T, size_t TotalSize = CACHE_LINE_SIZE>
struct Padded {
    static_assert(sizeof(T) <= TotalSize, "Type size exceeds total padded size");

    T value{};
    uint8_t padding[TotalSize - sizeof(T)]{};

    constexpr Padded() = default;
    constexpr explicit Padded(T val) : value(std::move(val)) {}

    constexpr T& get() noexcept { return value; }
    constexpr const T& get() const noexcept { return value; }

    constexpr T& operator*() noexcept { return value; }
    constexpr const T& operator*() const noexcept { return value; }
    constexpr T* operator->() noexcept { return &value; }
    constexpr const T* operator->() const noexcept { return &value; }
};

/**
 * @brief Hardware Cache-Control Intrinsics.
 */
class CacheController {
public:
    /**
     * @brief Flushes the cache line containing the specified address from all cache levels.
     */
    static inline void clflush(const void* p) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_clflush(p);
#else
        (void)p;
#endif
    }

    /**
     * @brief Optimized cache line write-back (CLWB / CLFLUSHOPT) if supported.
     */
    static inline void clflushopt(const void* p) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_clflushopt(const_cast<void*>(p));
#else
        (void)p;
#endif
    }

    /**
     * @brief Prefetches data into L1/L2/L3 cache hierarchy.
     */
    static inline void prefetch_t0(const void* p) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_prefetch(static_cast<const char*>(p), _MM_HINT_T0);
#else
        (void)p;
#endif
    }

    static inline void prefetch_nta(const void* p) noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_prefetch(static_cast<const char*>(p), _MM_HINT_NTA);
#else
        (void)p;
#endif
    }
};

/**
 * @brief Memory Layout and Hex-Dump Diagnostic Tools.
 */
class MemoryInspector {
public:
    /**
     * @brief Generates a canonical 16-byte hex dump of a raw memory range.
     * 
     * Output format:
     * 0x7ffd1234  00 01 02 03 04 05 06 07 | 08 09 0A 0B 0C 0D 0E 0F  |  ........ ........
     */
    static void hex_dump(const void* data, size_t size, std::ostream& os = std::cout) {
        if (!data || size == 0) {
            os << "[Empty or null buffer]\n";
            return;
        }

        const auto* bytes = static_cast<const uint8_t*>(data);
        const auto orig_flags = os.flags();
        const auto orig_fill = os.fill();

        for (size_t offset = 0; offset < size; offset += 16) {
            // Memory Address / Offset
            os << "0x" << std::hex << std::setw(8) << std::setfill('0')
               << reinterpret_cast<uintptr_t>(bytes + offset) << "  ";

            // Hex Bytes (Split into two 8-byte chunks)
            for (size_t i = 0; i < 16; ++i) {
                if (i == 8) os << "| ";
                if (offset + i < size) {
                    os << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<uint32_t>(bytes[offset + i]) << " ";
                } else {
                    os << "   ";
                }
            }

            // Printable ASCII representation
            os << " | ";
            for (size_t i = 0; i < 16; ++i) {
                if (i == 8) os << " ";
                if (offset + i < size) {
                    const uint8_t b = bytes[offset + i];
                    os << (std::isprint(b) ? static_cast<char>(b) : '.');
                } else {
                    os << " ";
                }
            }
            os << "\n";
        }

        os.flags(orig_flags);
        os.fill(orig_fill);
    }

    /**
     * @brief Formatted string version of hex_dump.
     */
    [[nodiscard]] static std::string hex_dump_str(const void* data, size_t size) {
        std::ostringstream oss;
        hex_dump(data, size, oss);
        return oss.str();
    }
};

/**
 * @brief Struct member layout and padding analyzer.
 */
struct FieldLayout {
    std::string_view name;
    size_t offset;
    size_t size;
    size_t alignment;
};

class StructLayoutInspector {
public:
    static void print_layout(std::string_view struct_name,
                             size_t total_size,
                             size_t struct_align,
                             const std::vector<FieldLayout>& fields,
                             std::ostream& os = std::cout) {
        os << "\n=======================================================\n"
           << " Struct Memory Layout: " << struct_name << "\n"
           << " Total Size: " << total_size << " bytes | Alignment: " << struct_align << " bytes\n"
           << "-------------------------------------------------------\n"
           << " Offset   Size  Align  Field Name                Padding\n"
           << "-------------------------------------------------------\n";

        size_t expected_offset = 0;
        size_t total_padding = 0;

        for (const auto& field : fields) {
            size_t padding = 0;
            if (field.offset > expected_offset) {
                padding = field.offset - expected_offset;
                total_padding += padding;
            }

            os << std::setw(6) << field.offset << "  "
               << std::setw(5) << field.size << "  "
               << std::setw(5) << field.alignment << "  "
               << std::left << std::setw(24) << field.name << std::right;

            if (padding > 0) {
                os << " (+" << padding << " padding)";
            }
            os << "\n";

            expected_offset = field.offset + field.size;
        }

        if (total_size > expected_offset) {
            const size_t tail_padding = total_size - expected_offset;
            total_padding += tail_padding;
            os << std::setw(6) << expected_offset << "  "
               << std::setw(5) << tail_padding << "     -  "
               << std::left << std::setw(24) << "[Tail Padding]" << std::right
               << " (+" << tail_padding << " padding)\n";
        }

        const double waste_pct = total_size > 0 
            ? (static_cast<double>(total_padding) / static_cast<double>(total_size)) * 100.0 
            : 0.0;

        os << "-------------------------------------------------------\n"
           << " Total Padding Waste: " << total_padding << " bytes (" 
           << std::fixed << std::setprecision(1) << waste_pct << "%)\n"
           << "=======================================================\n\n";
    }
};

} // namespace sys::mem

#define INSPECT_MEMBER(StructType, Member) \
    ::sys::mem::FieldLayout{ #Member, offsetof(StructType, Member), sizeof(std::declval<StructType>().Member), alignof(decltype(std::declval<StructType>().Member)) }
