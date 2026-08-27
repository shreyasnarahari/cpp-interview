#include "spsc_queue.hpp"
#include "mpmc_queue.hpp"
#include "../../../common/rdtsc_timer.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <iomanip>

void bench_spsc_throughput() {
    constexpr size_t TOTAL_MSGS = 10'000'000;
    std::cout << "========================================================================\n";
    std::cout << " Benchmark 1: Wait-Free SPSC Queue Throughput (" << TOTAL_MSGS << " Messages)\n";
    std::cout << "========================================================================\n\n";

    sys::lockfree::SPSCQueue<uint64_t, 65536> queue;
    std::atomic<bool> start_flag{false};

    std::thread producer([&] {
        while (!start_flag.load(std::memory_order_acquire)) {}
        for (uint64_t i = 1; i <= TOTAL_MSGS; ++i) {
            while (!queue.push(i)) {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
        }
    });

    uint64_t t0 = 0, t1 = 0;
    std::thread consumer([&] {
        while (!start_flag.load(std::memory_order_acquire)) {}
        t0 = sys::perf::HardwareCycleCounter::start_cycles();

        size_t count = 0;
        uint64_t val = 0;
        while (count < TOTAL_MSGS) {
            if (queue.pop(val)) {
                ++count;
            } else {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
        }
        t1 = sys::perf::HardwareCycleCounter::end_cycles();
    });

    start_flag.store(true, std::memory_order_release);
    producer.join();
    consumer.join();

    const auto& cal = sys::perf::TscCalibrator::instance();
    const double elapsed_ns = cal.cycles_to_ns(t1 - t0);
    const double elapsed_sec = elapsed_ns / 1e9;
    const double msgs_per_sec = static_cast<double>(TOTAL_MSGS) / elapsed_sec;
    const double million_msgs_per_sec = msgs_per_sec / 1e6;

    std::cout << "  Elapsed Time:     " << std::fixed << std::setprecision(2) << (elapsed_ns / 1e6) << " ms\n"
              << "  Throughput:       " << std::fixed << std::setprecision(2) << million_msgs_per_sec << " Million Msgs/sec\n"
              << "  Latency / Msg:    " << std::fixed << std::setprecision(2) << (elapsed_ns / TOTAL_MSGS) << " ns/msg\n\n";
}

void bench_spsc_ping_pong_latency() {
    constexpr size_t ROUNDTRIPS = 100'000;
    std::cout << "========================================================================\n";
    std::cout << " Benchmark 2: SPSC Ping-Pong Roundtrip Latency (" << ROUNDTRIPS << " Iterations)\n";
    std::cout << "========================================================================\n\n";

    sys::lockfree::SPSCQueue<uint64_t, 1024> ping_q;
    sys::lockfree::SPSCQueue<uint64_t, 1024> pong_q;
    std::atomic<bool> done{false};

    sys::perf::LatencyTracker tracker(ROUNDTRIPS);

    std::thread pong_thread([&] {
        uint64_t val = 0;
        while (!done.load(std::memory_order_relaxed)) {
            if (ping_q.pop(val)) {
                while (!pong_q.push(val)) {
                    #if defined(__x86_64__) || defined(_M_X64)
                    _mm_pause();
                    #endif
                }
            } else {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
        }
    });

    for (size_t i = 0; i < ROUNDTRIPS; ++i) {
        const uint64_t start = sys::perf::HardwareCycleCounter::start_cycles();
        while (!ping_q.push(i)) {}

        uint64_t res = 0;
        while (!pong_q.pop(res)) {
            #if defined(__x86_64__) || defined(_M_X64)
            _mm_pause();
            #endif
        }
        const uint64_t end = sys::perf::HardwareCycleCounter::end_cycles();
        tracker.record_cycles(end - start);
    }

    done.store(true, std::memory_order_relaxed);
    // Unblock pong thread if needed
    (void)ping_q.push(999999);
    pong_thread.join();

    tracker.print_summary("SPSC Queue Roundtrip Latency (Ping-Pong)", std::cout);
}

int main() {
    sys::perf::TscCalibrator::instance().calibrate(20);

    bench_spsc_throughput();
    bench_spsc_ping_pong_latency();

    return 0;
}
