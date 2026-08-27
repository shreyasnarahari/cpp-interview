#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>

namespace db::lsm {

/**
 * @brief High-Performance Bloom Filter with Kirsch-Mitzenmacher Double Hashing.
 */
class BloomFilter {
public:
    explicit BloomFilter(size_t expected_elements = 10000, double fpp = 0.01) {
        if (expected_elements == 0) expected_elements = 1;
        const double m = -static_cast<double>(expected_elements) * std::log(fpp) / (std::log(2.0) * std::log(2.0));
        num_bits_ = static_cast<size_t>(std::ceil(m));
        if (num_bits_ < 64) num_bits_ = 64;

        const double k = (static_cast<double>(num_bits_) / static_cast<double>(expected_elements)) * std::log(2.0);
        num_hashes_ = static_cast<size_t>(std::max(1.0, std::round(k)));

        bits_.resize((num_bits_ + 63) / 64, 0);
    }

    void add(uint64_t key) noexcept {
        const uint64_t h1 = hash1(key);
        const uint64_t h2 = hash2(key);

        for (size_t i = 0; i < num_hashes_; ++i) {
            const uint64_t combined = (h1 + i * h2) % num_bits_;
            bits_[combined / 64] |= (1ULL << (combined % 64));
        }
    }

    [[nodiscard]] bool contains(uint64_t key) const noexcept {
        const uint64_t h1 = hash1(key);
        const uint64_t h2 = hash2(key);

        for (size_t i = 0; i < num_hashes_; ++i) {
            const uint64_t combined = (h1 + i * h2) % num_bits_;
            if ((bits_[combined / 64] & (1ULL << (combined % 64))) == 0) {
                return false; // Definitely not present
            }
        }
        return true; // Possibly present
    }

    [[nodiscard]] const std::vector<uint64_t>& raw_bits() const noexcept { return bits_; }
    void set_raw_bits(std::vector<uint64_t> bits, size_t num_bits, size_t num_hashes) {
        bits_ = std::move(bits);
        num_bits_ = num_bits;
        num_hashes_ = num_hashes;
    }

    [[nodiscard]] size_t num_bits() const noexcept { return num_bits_; }
    [[nodiscard]] size_t num_hashes() const noexcept { return num_hashes_; }

private:
    static uint64_t hash1(uint64_t k) noexcept {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        return k;
    }

    static uint64_t hash2(uint64_t k) noexcept {
        k = (~k) + (k << 21);
        k = k ^ (k >> 24);
        k = (k + (k << 3)) + (k << 8);
        k = k ^ (k >> 14);
        k = (k + (k << 2)) + (k << 4);
        k = k ^ (k >> 28);
        k = k + (k << 31);
        return k;
    }

    size_t num_bits_{0};
    size_t num_hashes_{0};
    std::vector<uint64_t> bits_;
};

} // namespace db::lsm
