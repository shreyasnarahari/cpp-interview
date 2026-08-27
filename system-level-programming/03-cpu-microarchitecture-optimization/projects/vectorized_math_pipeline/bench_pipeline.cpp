#include "simd_vector_pipeline.hpp"
#include "../../exercises/market_tick.hpp"
#include "../../common/rdtsc_timer.hpp"
#include <iostream>
#include <vector>
#include <iomanip>

int main() {
    constexpr size_t N = 10'000'000;
    constexpr size_t TOTAL_BYTES = N * sizeof(double) * 2; // Read 1 array, write 1 array
    std::cout << "========================================================================\n";
    std::cout << " Benchmark: Streaming AVX2 Vector Math Pipeline (" << N << " Elements)\n";
    std::cout << " Total Data Streamed: " << (TOTAL_BYTES / (1024 * 1024)) << " MB\n";
    std::cout << "========================================================================\n\n";

    sys::perf::TscCalibrator::instance().calibrate(20);

    sys::simd::AlignedVector<double> src(N, 1.05);
    sys::simd::AlignedVector<double> dst(N, 0.0);

    // Warm up
    sys::simd::VectorPipeline::fma_stream(src.data(), dst.data(), 1000, 2.0, 1.0);

    constexpr size_t RUNS = 100;
    sys::perf::LatencyTracker tracker(RUNS);

    for (size_t r = 0; r < RUNS; ++r) {
        const uint64_t start = sys::perf::HardwareCycleCounter::start_cycles();
        sys::simd::VectorPipeline::fma_stream(src.data(), dst.data(), N, 2.0, 1.0);
        const uint64_t end = sys::perf::HardwareCycleCounter::end_cycles();
        tracker.record_cycles(end - start);
    }

    tracker.print_summary("AVX2 FMA Non-Temporal Streaming", std::cout);

    const auto stats = tracker.compute_stats();
    const double mean_seconds = (stats.mean_ns) / 1e9;
    const double giga_bytes = static_cast<double>(TOTAL_BYTES) / (1024.0 * 1024.0 * 1024.0);
    const double gb_per_sec = giga_bytes / mean_seconds;
    const double gflops = (static_cast<double>(N) * 2.0 / 1e9) / mean_seconds;

    std::cout << "------------------------------------------------------------------------\n"
              << " Mean Throughput: " << std::fixed << std::setprecision(2) << gb_per_sec << " GB/sec\n"
              << " Compute Rate:    " << std::fixed << std::setprecision(2) << gflops << " GFLOPS\n"
              << "------------------------------------------------------------------------\n\n";

    return 0;
}
