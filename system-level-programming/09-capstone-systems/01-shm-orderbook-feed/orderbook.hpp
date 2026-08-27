#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>
#include <optional>

namespace sys::hft {

enum class Side : uint8_t {
    BUY  = 1,
    SELL = 2
};

struct alignas(32) PriceLevel {
    double price{0.0};
    uint32_t qty{0};
    uint32_t order_count{0};
};

/**
 * @brief Ultra-Low Latency Fixed-Size Level 2 (L2) Limit Order Book.
 * 
 * Maintained with cacheline alignment and zero dynamic memory allocations.
 */
class L2OrderBook {
public:
    static constexpr size_t MAX_DEPTH = 32;

    constexpr L2OrderBook() noexcept = default;

    void update_level(Side side, double price, uint32_t qty, uint32_t count) noexcept {
        if (side == Side::BUY) {
            update_side(bids_, bid_count_, price, qty, count, [](double a, double b) { return a > b; });
        } else {
            update_side(asks_, ask_count_, price, qty, count, [](double a, double b) { return a < b; });
        }
    }

    [[nodiscard]] std::optional<PriceLevel> best_bid() const noexcept {
        if (bid_count_ == 0) return std::nullopt;
        return bids_[0];
    }

    [[nodiscard]] std::optional<PriceLevel> best_ask() const noexcept {
        if (ask_count_ == 0) return std::nullopt;
        return asks_[0];
    }

    [[nodiscard]] double mid_price() const noexcept {
        if (bid_count_ > 0 && ask_count_ > 0) {
            return (bids_[0].price + asks_[0].price) * 0.5;
        }
        if (bid_count_ > 0) return bids_[0].price;
        if (ask_count_ > 0) return asks_[0].price;
        return 0.0;
    }

    [[nodiscard]] double spread() const noexcept {
        if (bid_count_ > 0 && ask_count_ > 0) {
            return asks_[0].price - bids_[0].price;
        }
        return 0.0;
    }

    [[nodiscard]] size_t bid_depth() const noexcept { return bid_count_; }
    [[nodiscard]] size_t ask_depth() const noexcept { return ask_count_; }

    void clear() noexcept {
        bid_count_ = 0;
        ask_count_ = 0;
    }

private:
    template <typename Comparator>
    void update_side(std::array<PriceLevel, MAX_DEPTH>& levels, 
                     size_t& count, 
                     double price, 
                     uint32_t qty, 
                     uint32_t order_count,
                     Comparator cmp) noexcept {
        // Find existing level
        for (size_t i = 0; i < count; ++i) {
            if (levels[i].price == price) {
                if (qty == 0) {
                    // Remove level and shift down
                    for (size_t j = i; j + 1 < count; ++j) {
                        levels[j] = levels[j + 1];
                    }
                    --count;
                } else {
                    levels[i].qty = qty;
                    levels[i].order_count = order_count;
                }
                return;
            }
        }

        if (qty == 0) return; // Cannot insert empty level

        // Insert new level in sorted position
        size_t insert_pos = count;
        for (size_t i = 0; i < count; ++i) {
            if (cmp(price, levels[i].price)) {
                insert_pos = i;
                break;
            }
        }

        if (insert_pos < MAX_DEPTH) {
            const size_t limit = std::min(count + 1, MAX_DEPTH);
            for (size_t j = limit - 1; j > insert_pos; --j) {
                levels[j] = levels[j - 1];
            }
            levels[insert_pos] = PriceLevel{price, qty, order_count};
            if (count < MAX_DEPTH) {
                ++count;
            }
        }
    }

    alignas(64) std::array<PriceLevel, MAX_DEPTH> bids_{};
    alignas(64) std::array<PriceLevel, MAX_DEPTH> asks_{};
    size_t bid_count_{0};
    size_t ask_count_{0};
};

} // namespace sys::hft
