#include "wal_storage_engine.hpp"
#include "../../common/rdtsc_timer.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>

int main() {
    constexpr size_t OPS = 20'000;
    const std::string wal_file = "/tmp/bench_wal_engine.db";
    ::unlink(wal_file.c_str());

    std::cout << "========================================================================\n";
    std::cout << " Capstone 02: Append-Only WAL Storage Engine Benchmark (" << OPS << " Writes)\n";
    std::cout << " Record Size: 4096 Bytes (4 KB Sector Alignment)\n";
    std::cout << "========================================================================\n\n";

    sys::perf::TscCalibrator::instance().calibrate(20);

    sys::storage::WalStorageEngine engine(wal_file);
    assert(engine.open());

    const uint64_t t0 = sys::perf::HardwareCycleCounter::start_cycles();

    for (uint64_t i = 1; i <= OPS; ++i) {
        engine.put(i, i * 100);
    }

    const uint64_t t1 = sys::perf::HardwareCycleCounter::end_cycles();

    const double elapsed_ns = sys::perf::TscCalibrator::instance().cycles_to_ns(t1 - t0);
    const double elapsed_sec = elapsed_ns / 1e9;
    const double iops = static_cast<double>(OPS) / elapsed_sec;
    const double mb_written = (static_cast<double>(OPS * sizeof(sys::storage::WalBlock))) / (1024.0 * 1024.0);
    const double throughput_mb_s = mb_written / elapsed_sec;

    std::cout << "------------------------------------------------------------------------\n"
              << " Completed Writes:   " << OPS << " records\n"
              << " Total Written:      " << std::fixed << std::setprecision(2) << mb_written << " MB\n"
              << " Elapsed Time:       " << std::fixed << std::setprecision(2) << (elapsed_ns / 1e6) << " ms\n"
              << " Write IOPS:         " << std::fixed << std::setprecision(2) << iops << " ops/sec\n"
              << " Disk Throughput:    " << std::fixed << std::setprecision(2) << throughput_mb_s << " MB/sec\n"
              << " Latency / Write:    " << std::fixed << std::setprecision(2) << (elapsed_ns / OPS / 1000.0) << " us\n"
              << "------------------------------------------------------------------------\n\n";

    // Benchmark in-memory Read Latency
    sys::perf::LatencyTracker read_tracker(OPS);
    for (uint64_t i = 1; i <= OPS; ++i) {
        const uint64_t start = sys::perf::HardwareCycleCounter::start_cycles();
        auto val = engine.get(i);
        const uint64_t end = sys::perf::HardwareCycleCounter::end_cycles();
        (void)val;
        read_tracker.record_cycles(end - start);
    }

    read_tracker.print_summary("In-Memory Key Lookup Latency", std::cout);

    engine.close();
    ::unlink(wal_file.c_str());

    return 0;
}
