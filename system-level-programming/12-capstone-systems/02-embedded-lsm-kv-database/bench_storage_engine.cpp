#include "embedded_lsm_database.hpp"
#include "../../common/rdtsc_timer.hpp"
#include <iostream>
#include <iomanip>

int main() {
    constexpr size_t OPS = 50'000;
    const std::string db_dir = "/tmp/bench_embedded_lsm";
    std::filesystem::remove_all(db_dir);

    std::cout << "========================================================================\n";
    std::cout << " Capstone 02: Embedded LSM Key-Value Database Benchmark (" << OPS << " Ops)\n";
    std::cout << "========================================================================\n\n";

    sys::perf::TscCalibrator::instance().calibrate(20);

    sys::db::EmbeddedLsmDatabase db(db_dir, 64 * 1024);

    const uint64_t t0 = sys::perf::HardwareCycleCounter::start_cycles();

    for (uint64_t i = 1; i <= OPS; ++i) {
        db.put(i, i * 10);
    }

    const uint64_t t1 = sys::perf::HardwareCycleCounter::end_cycles();

    const double elapsed_ns = sys::perf::TscCalibrator::instance().cycles_to_ns(t1 - t0);
    const double elapsed_sec = elapsed_ns / 1e9;
    const double write_ops_sec = static_cast<double>(OPS) / elapsed_sec;

    std::cout << "------------------------------------------------------------------------\n"
              << " Completed Writes:   " << OPS << " keys\n"
              << " Flushed SSTables:   " << db.sstable_count() << "\n"
              << " Total Write Time:   " << std::fixed << std::setprecision(2) << (elapsed_ns / 1e6) << " ms\n"
              << " Write Throughput:   " << std::fixed << std::setprecision(2) << write_ops_sec << " ops/sec\n"
              << " Latency / Write:    " << std::fixed << std::setprecision(2) << (elapsed_ns / OPS) << " ns/write\n"
              << "------------------------------------------------------------------------\n\n";

    sys::perf::LatencyTracker read_tracker(OPS);
    for (uint64_t i = 1; i <= OPS; ++i) {
        const uint64_t start = sys::perf::HardwareCycleCounter::start_cycles();
        auto val = db.get(i);
        const uint64_t end = sys::perf::HardwareCycleCounter::end_cycles();
        (void)val;
        read_tracker.record_cycles(end - start);
    }

    read_tracker.print_summary("Embedded LSM Point Lookup Latency", std::cout);

    std::filesystem::remove_all(db_dir);
    return 0;
}
