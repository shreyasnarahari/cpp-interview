#include "common/rdtsc_timer.hpp"
#include "common/logger.hpp"
#include "common/memory_utils.hpp"

#include <iostream>
#include <vector>
#include <thread>
#include <cassert>

// Test structures for layout inspection
struct UnoptimizedOrder {
    char exchange_code;       // 1 byte
    // 7 bytes padding
    uint64_t order_id;        // 8 bytes
    uint8_t side;             // 1 byte
    // 3 bytes padding
    uint32_t quantity;        // 4 bytes
    double price;             // 8 bytes
    bool is_ioc;              // 1 byte
    // 7 bytes tail padding
};

struct alignas(64) OptimizedOrder {
    uint64_t order_id;        // 8 bytes
    double price;             // 8 bytes
    uint32_t quantity;        // 4 bytes
    char exchange_code;       // 1 byte
    uint8_t side;             // 1 byte
    bool is_ioc;              // 1 byte
    uint8_t reserved[41];     // padding out to 64 bytes cache line
};

void test_rdtsc_timer() {
    std::cout << "--- Testing RDTSC Timer & Calibration ---\n";
    auto& cal = sys::perf::TscCalibrator::instance();
    cal.calibrate(20);
    
    std::cout << "Calibrated TSC Frequency: " << cal.get_tsc_freq_ghz() << " GHz\n";
    assert(cal.get_tsc_freq_ghz() > 0.5 && cal.get_tsc_freq_ghz() < 10.0);

    sys::perf::LatencyTracker tracker(10'000);

    for (int i = 0; i < 1'000; ++i) {
        uint64_t start = sys::perf::HardwareCycleCounter::start_cycles();
        // Short busy loop with compiler memory barrier
        int sum = 0;
        for (int k = 0; k < 50; ++k) {
            sum += k;
            __asm__ __volatile__("" : "+r"(sum) : : "memory");
        }
        uint64_t end = sys::perf::HardwareCycleCounter::end_cycles();
        tracker.record_cycles(end - start);
    }

    auto stats = tracker.compute_stats();
    assert(stats.count == 1'000);
    assert(stats.min_cycles > 0);
    assert(stats.p50_cycles <= stats.p99_cycles);
    assert(stats.p99_cycles <= stats.max_cycles);

    tracker.print_summary("Busy Loop (50 iterations)", std::cout);
}

void test_logger() {
    std::cout << "--- Testing Zero-Allocation Lock-Free Logger ---\n";
    auto& log = sys::log::FastLogger<4096>::instance();
    log.set_log_level(sys::log::Level::DEBUG);

    bool ok1 = log.log(sys::log::Level::INFO, "OrderBook initialized with capacity: ", 1000000);
    bool ok2 = log.log(sys::log::Level::DEBUG, "Client connected from ID: ", 42, " with balance: $", 15420.75);
    bool ok3 = log.log(sys::log::Level::WARN, "High latency detected on gateway: ", "GW_NASDAQ_01");
    
    assert(ok1 && ok2 && ok3);

    size_t flushed = log.flush(STDOUT_FILENO);
    std::cout << "Flushed " << flushed << " log entries successfully.\n";
    assert(flushed >= 3);

    // Test signal safe crash logger
    std::cout << "Testing signal-safe crash logging format:\n";
    sys::log::SignalSafeLogger::log_crash(11, "SIGSEGV", reinterpret_cast<void*>(0xDEADBEEF00ULL));
}

void test_memory_utils() {
    std::cout << "--- Testing Memory Utilities & Alignment ---\n";

    sys::mem::CacheAligned<uint64_t> aligned_counter{12345};
    assert(sys::mem::is_aligned(&aligned_counter, 64));
    assert(*aligned_counter == 12345);

    sys::mem::Padded<uint32_t, 64> padded_val{999};
    assert(sizeof(padded_val) == 64);
    assert(*padded_val == 999);

    // Test Hex Dump
    std::cout << "\nMemory Hex Dump:\n";
    UnoptimizedOrder sample_order{'N', 9876543210ULL, 1, 500, 192.50, true};
    sys::mem::MemoryInspector::hex_dump(&sample_order, sizeof(sample_order), std::cout);

    // Test Struct Layout Inspector
    std::vector<sys::mem::FieldLayout> unopt_fields = {
        INSPECT_MEMBER(UnoptimizedOrder, exchange_code),
        INSPECT_MEMBER(UnoptimizedOrder, order_id),
        INSPECT_MEMBER(UnoptimizedOrder, side),
        INSPECT_MEMBER(UnoptimizedOrder, quantity),
        INSPECT_MEMBER(UnoptimizedOrder, price),
        INSPECT_MEMBER(UnoptimizedOrder, is_ioc)
    };

    sys::mem::StructLayoutInspector::print_layout(
        "UnoptimizedOrder", sizeof(UnoptimizedOrder), alignof(UnoptimizedOrder), unopt_fields, std::cout);

    std::vector<sys::mem::FieldLayout> opt_fields = {
        INSPECT_MEMBER(OptimizedOrder, order_id),
        INSPECT_MEMBER(OptimizedOrder, price),
        INSPECT_MEMBER(OptimizedOrder, quantity),
        INSPECT_MEMBER(OptimizedOrder, exchange_code),
        INSPECT_MEMBER(OptimizedOrder, side),
        INSPECT_MEMBER(OptimizedOrder, is_ioc),
        INSPECT_MEMBER(OptimizedOrder, reserved)
    };

    sys::mem::StructLayoutInspector::print_layout(
        "OptimizedOrder", sizeof(OptimizedOrder), alignof(OptimizedOrder), opt_fields, std::cout);

    // Test Cache Controller
    sys::mem::CacheController::prefetch_t0(&sample_order);
    sys::mem::CacheController::clflush(&sample_order);
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Running System-Level Programming Common Utilities Test\n";
    std::cout << "=======================================================\n\n";

    test_rdtsc_timer();
    test_logger();
    test_memory_utils();

    std::cout << "\n>>> ALL SYSTEM UTILITY CHECKS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
