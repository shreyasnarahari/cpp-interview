#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

// -------------------------------------------------------------
// Benchmark: False Sharing Demonstration
// Two threads increment independent counters. When counters
// share a cache line, performance degrades dramatically due
// to MESI coherency traffic (false sharing).
// -------------------------------------------------------------

static constexpr size_t ITERATIONS = 100'000'000;

// --- BAD: Both counters on the SAME cache line ---
struct SharedCacheLine {
    std::atomic<long long> counter_a{0};  // offset 0
    std::atomic<long long> counter_b{0};  // offset 8 — same 64-byte cache line!
};

// --- GOOD: Each counter on its OWN cache line ---
struct SeparateCacheLines {
    alignas(64) std::atomic<long long> counter_a{0};  // cache line 1
    alignas(64) std::atomic<long long> counter_b{0};  // cache line 2
};

template <typename Counters>
double benchmark_false_sharing(const char* label) {
    Counters counters{};

    auto start = std::chrono::high_resolution_clock::now();

    std::thread t1([&] {
        for (size_t i = 0; i < ITERATIONS; ++i) {
            counters.counter_a.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread t2([&] {
        for (size_t i = 0; i < ITERATIONS; ++i) {
            counters.counter_b.fetch_add(1, std::memory_order_relaxed);
        }
    });

    t1.join();
    t2.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    std::cout << "  [" << label << "] " << duration.count() << " ms"
              << "  (a=" << counters.counter_a.load()
              << ", b=" << counters.counter_b.load() << ")\n";

    return duration.count();
}

int main() {
    std::cout << "=== False Sharing Benchmark ===\n";
    std::cout << "Iterations per thread: " << ITERATIONS << "\n";
    std::cout << "Two threads increment independent atomic counters.\n\n";

    std::cout << "Struct sizes:\n";
    std::cout << "  SharedCacheLine:    " << sizeof(SharedCacheLine) << " bytes\n";
    std::cout << "  SeparateCacheLines: " << sizeof(SeparateCacheLines) << " bytes\n\n";

    // Warm up
    benchmark_false_sharing<SharedCacheLine>("warmup (shared)");
    benchmark_false_sharing<SeparateCacheLines>("warmup (separate)");
    std::cout << "\n";

    // Actual measurements
    std::cout << "Results (3 trials each):\n\n";
    std::cout << "Shared cache line (FALSE SHARING — slow):\n";
    double shared_total = 0;
    for (int i = 0; i < 3; ++i) {
        shared_total += benchmark_false_sharing<SharedCacheLine>("shared  ");
    }

    std::cout << "\nSeparate cache lines (NO false sharing — fast):\n";
    double separate_total = 0;
    for (int i = 0; i < 3; ++i) {
        separate_total += benchmark_false_sharing<SeparateCacheLines>("separate");
    }

    double shared_avg = shared_total / 3.0;
    double separate_avg = separate_total / 3.0;

    std::cout << "\n--- Summary ---\n";
    std::cout << "  Shared avg:   " << shared_avg << " ms\n";
    std::cout << "  Separate avg: " << separate_avg << " ms\n";
    std::cout << "  Speedup:      " << shared_avg / separate_avg << "x\n";
    std::cout << "\nFix: Use alignas(64) to put each thread's data on its own cache line.\n";

    return 0;
}
