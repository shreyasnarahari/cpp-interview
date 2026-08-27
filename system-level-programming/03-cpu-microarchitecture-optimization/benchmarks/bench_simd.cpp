#include "../exercises/market_tick.hpp"
#include "../common/rdtsc_timer.hpp"
#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <iomanip>
#include <cassert>

// Forward declarations
namespace sys::simd {
    double calculate_turnover_aos_scalar(const std::vector<AosMarketTick>& ticks) noexcept;
    double calculate_turnover_soa_scalar(const SoaTickDataset& dataset) noexcept;
    double calculate_turnover_soa_avx2(const SoaTickDataset& dataset) noexcept;
}

int main() {
    constexpr size_t NUM_TICKS = 10'000'000;
    std::cout << "========================================================================\n";
    std::cout << " Benchmark: AoS vs SoA vs AVX2 SIMD on " << NUM_TICKS << " Market Ticks\n";
    std::cout << "========================================================================\n\n";

    sys::perf::TscCalibrator::instance().calibrate(20);

    std::cout << "Generating " << NUM_TICKS << " synthetic market ticks (~320 MB memory)...\n";
    std::vector<sys::simd::AosMarketTick> aos_ticks(NUM_TICKS);
    sys::simd::SoaTickDataset soa_ticks(NUM_TICKS);

    std::mt19937_64 rng(1337);
    std::uniform_real_distribution<double> price_dist(100.0, 500.0);
    std::uniform_real_distribution<double> vol_dist(1.0, 1000.0);

    for (size_t i = 0; i < NUM_TICKS; ++i) {
        const double p = price_dist(rng);
        const double v = vol_dist(rng);

        aos_ticks[i] = sys::simd::AosMarketTick{p, v, i * 1000ULL, 1, {'A', 'A', 'P', 'L'}};
        soa_ticks.prices[i] = p;
        soa_ticks.volumes[i] = v;
        soa_ticks.timestamps[i] = i * 1000ULL;
        soa_ticks.flags[i] = 1;
    }

    std::cout << "Dataset generation complete. Executing benchmarks...\n\n";

    // 1. AoS Scalar Benchmark
    const uint64_t t0 = sys::perf::HardwareCycleCounter::start_cycles();
    const double res_aos = sys::simd::calculate_turnover_aos_scalar(aos_ticks);
    const uint64_t t1 = sys::perf::HardwareCycleCounter::end_cycles();

    // 2. SoA Scalar Benchmark
    const uint64_t t2 = sys::perf::HardwareCycleCounter::start_cycles();
    const double res_soa_scalar = sys::simd::calculate_turnover_soa_scalar(soa_ticks);
    const uint64_t t3 = sys::perf::HardwareCycleCounter::end_cycles();

    // 3. SoA AVX2 SIMD Benchmark
    const uint64_t t4 = sys::perf::HardwareCycleCounter::start_cycles();
    const double res_soa_avx2 = sys::simd::calculate_turnover_soa_avx2(soa_ticks);
    const uint64_t t5 = sys::perf::HardwareCycleCounter::end_cycles();

    const auto& cal = sys::perf::TscCalibrator::instance();
    const double ns_aos = cal.cycles_to_ns(t1 - t0);
    const double ns_soa = cal.cycles_to_ns(t3 - t2);
    const double ns_avx2 = cal.cycles_to_ns(t5 - t4);

    const double ms_aos = ns_aos / 1'000'000.0;
    const double ms_soa = ns_soa / 1'000'000.0;
    const double ms_avx2 = ns_avx2 / 1'000'000.0;

    // Verify numerical equivalence
    assert(std::abs(res_aos - res_soa_scalar) / res_aos < 1e-5);
    assert(std::abs(res_aos - res_soa_avx2) / res_aos < 1e-5);

    std::cout << "------------------------------------------------------------------------\n"
              << " Engine Type             Execution Time (ms)    Cycles / Element    Speedup\n"
              << "------------------------------------------------------------------------\n"
              << " 1. AoS Scalar           " << std::setw(15) << std::fixed << std::setprecision(2) << ms_aos << " ms    "
              << std::setw(15) << (static_cast<double>(t1 - t0) / NUM_TICKS) << " c/e    1.00x (Baseline)\n"
              << " 2. SoA Scalar           " << std::setw(15) << ms_soa << " ms    "
              << std::setw(15) << (static_cast<double>(t3 - t2) / NUM_TICKS) << " c/e    "
              << (ns_aos / ns_soa) << "x\n"
              << " 3. SoA AVX2 FMA Vector  " << std::setw(15) << ms_avx2 << " ms    "
              << std::setw(15) << (static_cast<double>(t5 - t4) / NUM_TICKS) << " c/e    "
              << (ns_aos / ns_avx2) << "x\n"
              << "------------------------------------------------------------------------\n\n";

    std::cout << ">>> AVX2 FMA processing achieves " << (ns_aos / ns_avx2) << "x total speedup over AoS baseline! <<<\n";
    return 0;
}
