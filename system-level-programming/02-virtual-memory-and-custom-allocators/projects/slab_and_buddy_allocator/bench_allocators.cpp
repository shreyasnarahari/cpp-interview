#include "slab_allocator.hpp"
#include "buddy_allocator.hpp"
#include "../../../common/rdtsc_timer.hpp"

#include <iostream>
#include <vector>
#include <cstdlib>

struct TradeRecord {
    uint64_t trade_id;
    double price;
    uint32_t qty;
    char symbol[8];
    uint64_t timestamp_ns;
};

void benchmark_slab_vs_malloc() {
    constexpr size_t ITERATIONS = 100'000;
    std::cout << "========================================================================\n";
    std::cout << " Benchmark: 64-Byte Object Allocation (Slab vs Standard malloc/free)\n";
    std::cout << " Iterations: " << ITERATIONS << "\n";
    std::cout << "========================================================================\n\n";

    sys::perf::TscCalibrator::instance().calibrate(20);

    sys::perf::LatencyTracker malloc_tracker(ITERATIONS);
    sys::perf::LatencyTracker slab_tracker(ITERATIONS);

    // Warm up
    for (size_t i = 0; i < 1000; ++i) {
        void* p = std::malloc(sizeof(TradeRecord));
        std::free(p);
    }

    // 1. Standard malloc/free Benchmark
    for (size_t i = 0; i < ITERATIONS; ++i) {
        const uint64_t start = sys::perf::HardwareCycleCounter::start_cycles();
        void* p = std::malloc(sizeof(TradeRecord));
        std::free(p);
        const uint64_t end = sys::perf::HardwareCycleCounter::end_cycles();
        malloc_tracker.record_cycles(end - start);
    }

    // 2. Slab Allocator Benchmark
    sys::alloc::SlabAllocator<TradeRecord, 4096> slab;
    for (size_t i = 0; i < ITERATIONS; ++i) {
        const uint64_t start = sys::perf::HardwareCycleCounter::start_cycles();
        void* p = slab.allocate();
        slab.deallocate(p);
        const uint64_t end = sys::perf::HardwareCycleCounter::end_cycles();
        slab_tracker.record_cycles(end - start);
    }

    malloc_tracker.print_summary("Standard std::malloc + std::free", std::cout);
    slab_tracker.print_summary("Slab Allocator (Intrusive Free-List)", std::cout);

    const auto malloc_stats = malloc_tracker.compute_stats();
    const auto slab_stats = slab_tracker.compute_stats();

    if (slab_stats.mean_ns > 0.0) {
        const double speedup = malloc_stats.mean_ns / slab_stats.mean_ns;
        std::cout << ">>> Slab Allocator is " << std::fixed << std::setprecision(2) 
                  << speedup << "x faster on average than std::malloc/free <<<\n\n";
    }
}

int main() {
    benchmark_slab_vs_malloc();
    return 0;
}
