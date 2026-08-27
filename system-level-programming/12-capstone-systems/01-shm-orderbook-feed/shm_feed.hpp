#pragma once

#include "orderbook.hpp"
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <string>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

namespace sys::hft {

enum class MarketEventType : uint8_t {
    UPDATE_LEVEL = 1,
    TRADE        = 2,
    CLEAR_BOOK   = 3
};

struct alignas(64) MarketEvent {
    uint64_t seq_num{0};
    uint64_t timestamp_ns{0};
    double price{0.0};
    uint32_t qty{0};
    uint32_t order_count{0};
    Side side{Side::BUY};
    MarketEventType event_type{MarketEventType::UPDATE_LEVEL};
};

struct alignas(64) ShmFeedHeader {
    alignas(64) std::atomic<uint64_t> write_index{0};
    alignas(64) std::atomic<uint64_t> read_index{0};
    uint64_t capacity{0};
    uint64_t mask{0};
};

class ShmMarketFeed {
public:
    static constexpr size_t DEFAULT_CAPACITY = 32768; // Power of 2

    static size_t calculate_size(size_t capacity) noexcept {
        return sizeof(ShmFeedHeader) + (capacity * sizeof(MarketEvent));
    }

    static ShmMarketFeed create(const std::string& shm_name, size_t capacity = DEFAULT_CAPACITY) {
        const size_t total_sz = calculate_size(capacity);
        int fd = ::shm_open(shm_name.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0666);
        if (fd < 0) throw std::runtime_error("Failed to create shm " + shm_name);
        if (::ftruncate(fd, static_cast<off_t>(total_sz)) < 0) {
            ::close(fd);
            throw std::runtime_error("ftruncate failed for " + shm_name);
        }
        void* ptr = ::mmap(nullptr, total_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        ::close(fd);
        if (ptr == MAP_FAILED) throw std::runtime_error("mmap failed for " + shm_name);

        auto* hdr = static_cast<ShmFeedHeader*>(ptr);
        hdr->write_index.store(0, std::memory_order_relaxed);
        hdr->read_index.store(0, std::memory_order_relaxed);
        hdr->capacity = capacity;
        hdr->mask = capacity - 1;

        return ShmMarketFeed(shm_name, ptr, total_sz, true);
    }

    static ShmMarketFeed attach(const std::string& shm_name, size_t capacity = DEFAULT_CAPACITY) {
        const size_t total_sz = calculate_size(capacity);
        int fd = ::shm_open(shm_name.c_str(), O_RDWR, 0666);
        if (fd < 0) throw std::runtime_error("Failed to attach shm " + shm_name);
        void* ptr = ::mmap(nullptr, total_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        ::close(fd);
        if (ptr == MAP_FAILED) throw std::runtime_error("mmap attach failed for " + shm_name);

        return ShmMarketFeed(shm_name, ptr, total_sz, false);
    }

    ~ShmMarketFeed() {
        if (mapped_base_) {
            ::munmap(mapped_base_, total_size_);
            if (is_creator_) ::shm_unlink(shm_name_.c_str());
        }
    }

    ShmMarketFeed(const ShmMarketFeed&) = delete;
    ShmMarketFeed& operator=(const ShmMarketFeed&) = delete;

    ShmMarketFeed(ShmMarketFeed&& other) noexcept
        : shm_name_(std::move(other.shm_name_)),
          mapped_base_(other.mapped_base_),
          total_size_(other.total_size_),
          is_creator_(other.is_creator_),
          header_(other.header_),
          ring_(other.ring_) {
        other.mapped_base_ = nullptr;
    }

    [[nodiscard]] bool publish(const MarketEvent& event) noexcept {
        const uint64_t w = header_->write_index.load(std::memory_order_relaxed);
        const uint64_t r = header_->read_index.load(std::memory_order_acquire);

        if (w - r >= header_->capacity) {
            return false; // Queue full
        }

        ring_[w & header_->mask] = event;
        header_->write_index.store(w + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool consume(MarketEvent& out) noexcept {
        const uint64_t r = header_->read_index.load(std::memory_order_relaxed);
        const uint64_t w = header_->write_index.load(std::memory_order_acquire);

        if (r == w) {
            return false; // Queue empty
        }

        out = ring_[r & header_->mask];
        header_->read_index.store(r + 1, std::memory_order_release);
        return true;
    }

private:
    ShmMarketFeed(std::string name, void* base, size_t size, bool creator)
        : shm_name_(std::move(name)),
          mapped_base_(base),
          total_size_(size),
          is_creator_(creator),
          header_(static_cast<ShmFeedHeader*>(base)),
          ring_(reinterpret_cast<MarketEvent*>(static_cast<uint8_t*>(base) + sizeof(ShmFeedHeader))) {}

    std::string shm_name_;
    void* mapped_base_{nullptr};
    size_t total_size_{0};
    bool is_creator_{false};
    ShmFeedHeader* header_{nullptr};
    MarketEvent* ring_{nullptr};
};

} // namespace sys::hft
