#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <string_view>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <string>

namespace sys::ipc {

/**
 * @brief Fixed-Size 128-Byte Binary IPC Message Frame.
 */
struct alignas(64) IpcMessage {
    static constexpr size_t MAX_PAYLOAD = 104;

    uint64_t msg_id{0};
    uint64_t timestamp_ns{0};
    uint32_t length{0};
    char payload[MAX_PAYLOAD]{0};
};

/**
 * @brief POSIX Shared Memory Ring Buffer Header.
 */
struct alignas(64) ShmRingHeader {
    alignas(64) std::atomic<uint64_t> write_index{0};
    alignas(64) std::atomic<uint64_t> read_index{0};
    uint64_t capacity{0};
    uint64_t mask{0};
};

/**
 * @brief Ultra-Low Latency POSIX Shared Memory IPC Bus.
 */
class ShmIpcBus {
public:
    static constexpr size_t DEFAULT_CAPACITY = 16384; // Power of 2

    static size_t calculate_shm_size(size_t capacity) noexcept {
        return sizeof(ShmRingHeader) + (capacity * sizeof(IpcMessage));
    }

    /**
     * @brief Creates and initializes a new shared memory IPC bus (Producer role).
     */
    static ShmIpcBus create(const std::string& shm_name, size_t capacity = DEFAULT_CAPACITY) {
        const size_t total_size = calculate_shm_size(capacity);

        int fd = ::shm_open(shm_name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
        if (fd < 0) {
            throw std::runtime_error("shm_open create failed for " + shm_name);
        }

        if (::ftruncate(fd, static_cast<off_t>(total_size)) < 0) {
            ::close(fd);
            throw std::runtime_error("ftruncate failed for " + shm_name);
        }

        void* mapped = ::mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        ::close(fd);

        if (mapped == MAP_FAILED) {
            throw std::runtime_error("mmap failed for " + shm_name);
        }

        auto* header = static_cast<ShmRingHeader*>(mapped);
        header->write_index.store(0, std::memory_order_relaxed);
        header->read_index.store(0, std::memory_order_relaxed);
        header->capacity = capacity;
        header->mask = capacity - 1;

        return ShmIpcBus(shm_name, mapped, total_size, true);
    }

    /**
     * @brief Connects to an existing shared memory IPC bus (Consumer role).
     */
    static ShmIpcBus attach(const std::string& shm_name, size_t capacity = DEFAULT_CAPACITY) {
        const size_t total_size = calculate_shm_size(capacity);

        int fd = ::shm_open(shm_name.c_str(), O_RDWR, 0666);
        if (fd < 0) {
            throw std::runtime_error("shm_open attach failed for " + shm_name);
        }

        void* mapped = ::mmap(nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        ::close(fd);

        if (mapped == MAP_FAILED) {
            throw std::runtime_error("mmap attach failed for " + shm_name);
        }

        return ShmIpcBus(shm_name, mapped, total_size, false);
    }

    ~ShmIpcBus() {
        if (mapped_base_) {
            ::munmap(mapped_base_, total_size_);
            if (is_creator_) {
                ::shm_unlink(shm_name_.c_str());
            }
        }
    }

    ShmIpcBus(const ShmIpcBus&) = delete;
    ShmIpcBus& operator=(const ShmIpcBus&) = delete;

    ShmIpcBus(ShmIpcBus&& other) noexcept
        : shm_name_(std::move(other.shm_name_)),
          mapped_base_(other.mapped_base_),
          total_size_(other.total_size_),
          is_creator_(other.is_creator_),
          header_(other.header_),
          ring_(other.ring_) {
        other.mapped_base_ = nullptr;
    }

    ShmIpcBus& operator=(ShmIpcBus&& other) noexcept {
        if (this != &other) {
            if (mapped_base_) {
                ::munmap(mapped_base_, total_size_);
                if (is_creator_) ::shm_unlink(shm_name_.c_str());
            }
            shm_name_ = std::move(other.shm_name_);
            mapped_base_ = other.mapped_base_;
            total_size_ = other.total_size_;
            is_creator_ = other.is_creator_;
            header_ = other.header_;
            ring_ = other.ring_;
            other.mapped_base_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief Publishes a message to the shared memory ring buffer.
     */
    [[nodiscard]] bool publish(uint64_t msg_id, uint64_t ts_ns, std::string_view payload) noexcept {
        const uint64_t w = header_->write_index.load(std::memory_order_relaxed);
        const uint64_t r = header_->read_index.load(std::memory_order_acquire);

        if (w - r >= header_->capacity) {
            return false; // Ring buffer full
        }

        IpcMessage& slot = ring_[w & header_->mask];
        slot.msg_id = msg_id;
        slot.timestamp_ns = ts_ns;
        const size_t len = std::min(payload.size(), IpcMessage::MAX_PAYLOAD);
        slot.length = static_cast<uint32_t>(len);
        std::memcpy(slot.payload, payload.data(), len);

        header_->write_index.store(w + 1, std::memory_order_release);
        return true;
    }

    /**
     * @brief Consumes the next available message from the shared memory ring buffer.
     */
    [[nodiscard]] bool consume(IpcMessage& out) noexcept {
        const uint64_t r = header_->read_index.load(std::memory_order_relaxed);
        const uint64_t w = header_->write_index.load(std::memory_order_acquire);

        if (r == w) {
            return false; // Ring buffer empty
        }

        const IpcMessage& slot = ring_[r & header_->mask];
        out = slot;

        header_->read_index.store(r + 1, std::memory_order_release);
        return true;
    }

private:
    ShmIpcBus(std::string name, void* base, size_t size, bool creator)
        : shm_name_(std::move(name)),
          mapped_base_(base),
          total_size_(size),
          is_creator_(creator),
          header_(static_cast<ShmRingHeader*>(base)),
          ring_(reinterpret_cast<IpcMessage*>(static_cast<uint8_t*>(base) + sizeof(ShmRingHeader))) {}

    std::string shm_name_;
    void* mapped_base_{nullptr};
    size_t total_size_{0};
    bool is_creator_{false};
    ShmRingHeader* header_{nullptr};
    IpcMessage* ring_{nullptr};
};

} // namespace sys::ipc
