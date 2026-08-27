#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <atomic>
#include <string_view>
#include <cstring>
#include <chrono>
#include <type_traits>
#include <unistd.h>
#include <sys/types.h>

namespace sys::log {

/**
 * @brief Severity log levels.
 */
enum class Level : uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    FATAL = 5
};

[[nodiscard]] constexpr const char* to_string(Level lvl) noexcept {
    switch (lvl) {
        case Level::TRACE: return "[TRACE]";
        case Level::DEBUG: return "[DEBUG]";
        case Level::INFO:  return "[INFO ]";
        case Level::WARN:  return "[WARN ]";
        case Level::ERROR: return "[ERROR]";
        case Level::FATAL: return "[FATAL]";
        default:           return "[UNK  ]";
    }
}

/**
 * @brief Ultra-fast, zero-allocation ASCII and integer formatting utilities.
 */
namespace detail {

    inline constexpr char DIGITS[201] =
        "0001020304050607080910111213141516171819"
        "2021222324252627282930313233343536373839"
        "4041424344454647484950515253545556575859"
        "6061626364656667686970717273747576777879"
        "8081828384858687888990919293949596979899";

    inline constexpr char HEX_DIGITS[] = "0123456789ABCDEF";

    inline char* write_uint64(char* dst, uint64_t val) noexcept {
        char temp[24];
        char* p = temp + sizeof(temp);
        *--p = '\0';

        if (val == 0) {
            *--p = '0';
        } else {
            while (val >= 100) {
                const uint64_t rem = val % 100;
                val /= 100;
                p -= 2;
                std::memcpy(p, &DIGITS[rem * 2], 2);
            }
            if (val < 10) {
                *--p = static_cast<char>('0' + val);
            } else {
                p -= 2;
                std::memcpy(p, &DIGITS[val * 2], 2);
            }
        }

        const size_t len = static_cast<size_t>((temp + sizeof(temp)) - p - 1);
        std::memcpy(dst, p, len);
        return dst + len;
    }

    inline char* write_int64(char* dst, int64_t val) noexcept {
        if (val < 0) {
            *dst++ = '-';
            val = -val;
        }
        return write_uint64(dst, static_cast<uint64_t>(val));
    }

    inline char* write_hex64(char* dst, uint64_t val) noexcept {
        *dst++ = '0';
        *dst++ = 'x';
        bool leading_zero = true;
        for (int shift = 60; shift >= 0; shift -= 4) {
            const uint8_t nibble = static_cast<uint8_t>((val >> shift) & 0x0F);
            if (nibble != 0 || !leading_zero || shift == 0) {
                leading_zero = false;
                *dst++ = HEX_DIGITS[nibble];
            }
        }
        return dst;
    }

    inline char* write_str(char* dst, std::string_view sv) noexcept {
        std::memcpy(dst, sv.data(), sv.size());
        return dst + sv.size();
    }
} // namespace detail

/**
 * @brief Async-Signal-Safe Crash Logger.
 * 
 * Invoked during fatal signal handling (e.g. SIGSEGV, SIGABRT, SIGFPE).
 * Directly invokes ::write() system call without heap allocation or mutexes.
 */
class SignalSafeLogger {
public:
    static void write_raw(int fd, const char* str, size_t len) noexcept {
        while (len > 0) {
            const ssize_t written = ::write(fd, str, len);
            if (written <= 0) {
                break;
            }
            str += written;
            len -= static_cast<size_t>(written);
        }
    }

    static void write_str(int fd, const char* str) noexcept {
        if (str) {
            write_raw(fd, str, std::strlen(str));
        }
    }

    static void write_uint(int fd, uint64_t val) noexcept {
        char buf[32];
        char* end = detail::write_uint64(buf, val);
        write_raw(fd, buf, static_cast<size_t>(end - buf));
    }

    static void write_hex(int fd, uint64_t val) noexcept {
        char buf[32];
        char* end = detail::write_hex64(buf, val);
        write_raw(fd, buf, static_cast<size_t>(end - buf));
    }

    static void log_crash(int sig, const char* signame, void* fault_addr) noexcept {
        write_str(STDERR_FILENO, "\n=======================================================\n");
        write_str(STDERR_FILENO, " [FATAL SIGNAL] Caught signal: ");
        write_uint(STDERR_FILENO, static_cast<uint64_t>(sig));
        write_str(STDERR_FILENO, " (");
        write_str(STDERR_FILENO, signame ? signame : "UNKNOWN");
        write_str(STDERR_FILENO, ")\n Faulting Memory Address: ");
        write_hex(STDERR_FILENO, reinterpret_cast<uintptr_t>(fault_addr));
        write_str(STDERR_FILENO, "\n=======================================================\n");
    }
};

/**
 * @brief Fixed-size, zero-allocation log entry buffer.
 */
struct alignas(64) LogMessage {
    static constexpr size_t MAX_PAYLOAD_SIZE = 256;
    
    uint64_t timestamp_ns{0};
    Level level{Level::INFO};
    uint16_t length{0};
    char payload[MAX_PAYLOAD_SIZE]{0};
};

/**
 * @brief High-Throughput Zero-Allocation Lock-Free SPSC / MPMC Ring Buffer Logger.
 */
template <size_t Capacity = 4096>
class FastLogger {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    static FastLogger& instance() noexcept {
        static FastLogger logger;
        return logger;
    }

    void set_log_level(Level lvl) noexcept {
        min_level_.store(lvl, std::memory_order_relaxed);
    }

    [[nodiscard]] Level get_log_level() const noexcept {
        return min_level_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Formats and writes a log record into the lock-free ring buffer.
     */
    template <typename... Args>
    bool log(Level lvl, Args&&... args) noexcept {
        if (static_cast<uint8_t>(lvl) < static_cast<uint8_t>(min_level_.load(std::memory_order_relaxed))) {
            return false;
        }

        const uint64_t current_tail = tail_.load(std::memory_order_relaxed);
        const uint64_t current_head = head_.load(std::memory_order_acquire);

        if (current_tail - current_head >= Capacity) {
            // Buffer full: drop or write directly to fallback
            dropped_count_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        LogMessage& entry = ring_[current_tail & MASK];
        entry.timestamp_ns = get_timestamp_ns();
        entry.level = lvl;

        char* ptr = entry.payload;
        char* end_ptr = entry.payload + (LogMessage::MAX_PAYLOAD_SIZE - 2);

        // Format all arguments
        (format_arg(ptr, end_ptr, std::forward<Args>(args)), ...);
        
        *ptr++ = '\n';
        entry.length = static_cast<uint16_t>(ptr - entry.payload);

        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Flushes all queued log entries to standard error or a file descriptor.
     */
    size_t flush(int fd = STDERR_FILENO) noexcept {
        size_t flushed = 0;
        uint64_t current_head = head_.load(std::memory_order_relaxed);
        const uint64_t current_tail = tail_.load(std::memory_order_acquire);

        while (current_head < current_tail) {
            const LogMessage& entry = ring_[current_head & MASK];
            
            // Format header: [TIMESTAMP] [LEVEL] MESSAGE
            char hdr[64];
            char* hptr = hdr;
            *hptr++ = '[';
            hptr = detail::write_uint64(hptr, entry.timestamp_ns);
            *hptr++ = ']';
            *hptr++ = ' ';
            hptr = detail::write_str(hptr, to_string(entry.level));
            *hptr++ = ' ';

            SignalSafeLogger::write_raw(fd, hdr, static_cast<size_t>(hptr - hdr));
            SignalSafeLogger::write_raw(fd, entry.payload, entry.length);

            ++current_head;
            ++flushed;
        }

        head_.store(current_head, std::memory_order_release);
        return flushed;
    }

    [[nodiscard]] uint64_t get_dropped_count() const noexcept {
        return dropped_count_.load(std::memory_order_relaxed);
    }

private:
    FastLogger() noexcept = default;

    static uint64_t get_timestamp_ns() noexcept {
        struct timespec ts;
        ::clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);
    }

    template <typename T>
    static void format_arg(char*& ptr, char* end_ptr, T&& val) noexcept {
        if (ptr >= end_ptr) return;

        using Decayed = std::decay_t<T>;
        if constexpr (std::is_same_v<Decayed, const char*> || std::is_same_v<Decayed, char*>) {
            const size_t len = std::strlen(val);
            const size_t to_copy = std::min(len, static_cast<size_t>(end_ptr - ptr));
            std::memcpy(ptr, val, to_copy);
            ptr += to_copy;
        } else if constexpr (std::is_same_v<Decayed, std::string_view>) {
            const size_t to_copy = std::min(val.size(), static_cast<size_t>(end_ptr - ptr));
            std::memcpy(ptr, val.data(), to_copy);
            ptr += to_copy;
        } else if constexpr (std::is_integral_v<Decayed>) {
            if constexpr (std::is_signed_v<Decayed>) {
                ptr = detail::write_int64(ptr, static_cast<int64_t>(val));
            } else {
                ptr = detail::write_uint64(ptr, static_cast<uint64_t>(val));
            }
        } else if constexpr (std::is_pointer_v<Decayed>) {
            ptr = detail::write_hex64(ptr, reinterpret_cast<uintptr_t>(val));
        } else if constexpr (std::is_floating_point_v<Decayed>) {
            // Fast integer conversion for floating point approximation
            const int64_t integer_part = static_cast<int64_t>(val);
            const int64_t frac_part = static_cast<int64_t>(std::abs(val - static_cast<double>(integer_part)) * 1000.0);
            ptr = detail::write_int64(ptr, integer_part);
            *ptr++ = '.';
            if (frac_part < 100) *ptr++ = '0';
            if (frac_part < 10) *ptr++ = '0';
            ptr = detail::write_int64(ptr, frac_part);
        }
    }

    static constexpr size_t MASK = Capacity - 1;

    alignas(64) std::atomic<uint64_t> head_{0};
    alignas(64) std::atomic<uint64_t> tail_{0};
    alignas(64) std::atomic<uint64_t> dropped_count_{0};
    alignas(64) std::atomic<Level> min_level_{Level::INFO};

    alignas(64) std::array<LogMessage, Capacity> ring_{};
};

// Convenience log macros
#define LOG_TRACE(...) ::sys::log::FastLogger<4096>::instance().log(::sys::log::Level::TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) ::sys::log::FastLogger<4096>::instance().log(::sys::log::Level::DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  ::sys::log::FastLogger<4096>::instance().log(::sys::log::Level::INFO,  __VA_ARGS__)
#define LOG_WARN(...)  ::sys::log::FastLogger<4096>::instance().log(::sys::log::Level::WARN,  __VA_ARGS__)
#define LOG_ERROR(...) ::sys::log::FastLogger<4096>::instance().log(::sys::log::Level::ERROR, __VA_ARGS__)
#define LOG_FATAL(...) ::sys::log::FastLogger<4096>::instance().log(::sys::log::Level::FATAL, __VA_ARGS__)

} // namespace sys::log
