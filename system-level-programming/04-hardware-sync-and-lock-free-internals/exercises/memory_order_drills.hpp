#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <thread>
#include <iostream>
#include <cassert>

namespace sys::sync {

struct OrderData {
    uint64_t order_id{0};
    double price{0.0};
    uint32_t qty{0};
    bool valid{false};
};

/**
 * @brief Canonical Acquire-Release Message Passing Synchronization.
 * 
 * Guarantees that all writes to `data_` are completely visible
 * to the consumer before `ready_` is observed as true.
 */
class AcquireReleaseChannel {
public:
    [[nodiscard]] bool try_send(uint64_t id, double p, uint32_t q) noexcept {
        // Wait until consumer has consumed previous message
        if (has_data_.load(std::memory_order_acquire)) {
            return false;
        }

        // Step 1: Write non-atomic payload
        data_.order_id = id;
        data_.price = p;
        data_.qty = q;
        data_.valid = true;

        // Step 2: Publish with release ordering
        has_data_.store(true, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_receive(OrderData& out) noexcept {
        // Step 1: Check flag with acquire ordering
        if (!has_data_.load(std::memory_order_acquire)) {
            return false;
        }

        // Step 2: Read published payload safely
        out = data_;

        // Step 3: Acknowledge consumption with release ordering
        has_data_.store(false, std::memory_order_release);
        return true;
    }

private:
    OrderData data_{};
    alignas(64) std::atomic<bool> has_data_{false};
};

} // namespace sys::sync
