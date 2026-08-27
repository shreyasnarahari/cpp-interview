#include "market_tick.hpp"
#include <iostream>
#include <vector>
#include <numeric>
#include <immintrin.h>

namespace sys::simd {

/**
 * @brief 1. Scalar AoS Turnover Calculation (Poor Cache Locality).
 */
[[nodiscard]] double calculate_turnover_aos_scalar(const std::vector<AosMarketTick>& ticks) noexcept {
    double total = 0.0;
    for (const auto& tick : ticks) {
        total += tick.price * tick.volume;
    }
    return total;
}

/**
 * @brief 2. Scalar SoA Turnover Calculation (Contiguous Memory Traversals).
 */
[[nodiscard]] double calculate_turnover_soa_scalar(const SoaTickDataset& dataset) noexcept {
    double total = 0.0;
    const double* const __restrict prices = dataset.prices.data();
    const double* const __restrict volumes = dataset.volumes.data();
    const size_t n = dataset.size();

    for (size_t i = 0; i < n; ++i) {
        total += prices[i] * volumes[i];
    }
    return total;
}

/**
 * @brief 3. Hand-Crafted AVX2 FMA Vectorized SoA Calculation with 4x Loop Unrolling.
 */
[[nodiscard]] double calculate_turnover_soa_avx2(const SoaTickDataset& dataset) noexcept {
    const double* const __restrict prices = dataset.prices.data();
    const double* const __restrict volumes = dataset.volumes.data();
    const size_t n = dataset.size();

    __m256d sum0 = _mm256_setzero_pd();
    __m256d sum1 = _mm256_setzero_pd();
    __m256d sum2 = _mm256_setzero_pd();
    __m256d sum3 = _mm256_setzero_pd();

    size_t i = 0;
    // Process 16 doubles per iteration (4 x 256-bit registers)
    for (; i + 15 < n; i += 16) {
        __m256d p0 = _mm256_load_pd(prices + i);
        __m256d v0 = _mm256_load_pd(volumes + i);
        sum0 = _mm256_fmadd_pd(p0, v0, sum0);

        __m256d p1 = _mm256_load_pd(prices + i + 4);
        __m256d v1 = _mm256_load_pd(volumes + i + 4);
        sum1 = _mm256_fmadd_pd(p1, v1, sum1);

        __m256d p2 = _mm256_load_pd(prices + i + 8);
        __m256d v2 = _mm256_load_pd(volumes + i + 8);
        sum2 = _mm256_fmadd_pd(p2, v2, sum2);

        __m256d p3 = _mm256_load_pd(prices + i + 12);
        __m256d v3 = _mm256_load_pd(volumes + i + 12);
        sum3 = _mm256_fmadd_pd(p3, v3, sum3);
    }

    // Combine 4 vector accumulators
    __m256d sum = _mm256_add_pd(_mm256_add_pd(sum0, sum1), _mm256_add_pd(sum2, sum3));

    // Horizontal reduction of 256-bit sum vector
    alignas(32) double temp[4];
    _mm256_store_pd(temp, sum);
    double total = temp[0] + temp[1] + temp[2] + temp[3];

    // Process remaining tail elements
    for (; i < n; ++i) {
        total += prices[i] * volumes[i];
    }

    return total;
}

} // namespace sys::simd
