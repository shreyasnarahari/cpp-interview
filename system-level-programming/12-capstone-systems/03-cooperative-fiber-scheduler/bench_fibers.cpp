#include "fiber_scheduler.hpp"
#include "../../common/rdtsc_timer.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>

void bench_fiber_context_switches() {
    constexpr size_t SWITCHES = 500'000;
    std::cout << "========================================================================\n";
    std::cout << " Capstone 03: Fiber Context Switch Latency Benchmark (" << SWITCHES << " Yields)\n";
    std::cout << "========================================================================\n\n";

    sys::fiber::Scheduler sched;
    sys::perf::LatencyTracker tracker(100'000);

    size_t count = 0;
    sched.spawn([&] {
        while (count < SWITCHES) {
            const uint64_t t0 = sys::perf::HardwareCycleCounter::start_cycles();
            sys::fiber::Scheduler::yield();
            const uint64_t t1 = sys::perf::HardwareCycleCounter::end_cycles();
            if (tracker.size() < 100'000) {
                tracker.record_cycles(t1 - t0);
            }
            ++count;
        }
    });

    sched.spawn([&] {
        while (count < SWITCHES) {
            sys::fiber::Scheduler::yield();
        }
    });

    sched.run();

    tracker.print_summary("Cooperative Fiber Context Switch Latency", std::cout);
}

void bench_fiber_throughput() {
    constexpr size_t FIBER_COUNT = 50'000;
    std::cout << "========================================================================\n";
    std::cout << " Capstone 03: Fiber Spawn & Execution Throughput (" << FIBER_COUNT << " Fibers)\n";
    std::cout << "========================================================================\n\n";

    sys::fiber::Scheduler sched;
    size_t executed = 0;

    const uint64_t t0 = sys::perf::HardwareCycleCounter::start_cycles();

    for (size_t i = 0; i < FIBER_COUNT; ++i) {
        sched.spawn([&] {
            ++executed;
        });
    }

    sched.run();

    const uint64_t t1 = sys::perf::HardwareCycleCounter::end_cycles();

    const double elapsed_ns = sys::perf::TscCalibrator::instance().cycles_to_ns(t1 - t0);
    const double elapsed_sec = elapsed_ns / 1e9;
    const double fibers_per_sec = static_cast<double>(FIBER_COUNT) / elapsed_sec;

    std::cout << "  Executed Fibers: " << executed << "\n"
              << "  Elapsed Time:    " << std::fixed << std::setprecision(2) << (elapsed_ns / 1e6) << " ms\n"
              << "  Throughput:      " << std::fixed << std::setprecision(2) << (fibers_per_sec / 1e3) << "k fibers/sec\n"
              << "  Time / Fiber:    " << std::fixed << std::setprecision(2) << (elapsed_ns / FIBER_COUNT) << " ns/fiber\n\n";
}

int main() {
    sys::perf::TscCalibrator::instance().calibrate(20);

    bench_fiber_context_switches();
    bench_fiber_throughput();

    return 0;
}
