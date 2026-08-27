#include "shm_ipc_bus.hpp"
#include "../../../common/rdtsc_timer.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cassert>
#include <iomanip>
#include <immintrin.h>

int main() {
    constexpr size_t ROUNDTRIPS = 50'000;
    const std::string ping_shm = "/shm_bench_ping";
    const std::string pong_shm = "/shm_bench_pong";

    std::cout << "========================================================================\n";
    std::cout << " Benchmark: Cross-Process Shared Memory Ping-Pong Latency (" << ROUNDTRIPS << " Iterations)\n";
    std::cout << "========================================================================\n\n";

    sys::perf::TscCalibrator::instance().calibrate(20);

    auto ping_bus = sys::ipc::ShmIpcBus::create(ping_shm, 1024);
    auto pong_bus = sys::ipc::ShmIpcBus::create(pong_shm, 1024);

    pid_t pid = ::fork();
    assert(pid >= 0);

    if (pid == 0) {
        // Child Process: Echoes messages from ping_bus to pong_bus
        auto child_ping = sys::ipc::ShmIpcBus::attach(ping_shm, 1024);
        auto child_pong = sys::ipc::ShmIpcBus::attach(pong_shm, 1024);

        for (size_t i = 0; i < ROUNDTRIPS; ++i) {
            sys::ipc::IpcMessage msg;
            while (!child_ping.consume(msg)) {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
            while (!child_pong.publish(msg.msg_id, msg.timestamp_ns, "PONG")) {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
        }
        ::_exit(0);
    } else {
        // Parent Process: Sends ping, measures roundtrip latency until pong
        sys::perf::LatencyTracker tracker(ROUNDTRIPS);

        for (uint64_t i = 0; i < ROUNDTRIPS; ++i) {
            const uint64_t start = sys::perf::HardwareCycleCounter::start_cycles();
            while (!ping_bus.publish(i, start, "PING")) {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }

            sys::ipc::IpcMessage reply;
            while (!pong_bus.consume(reply)) {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
            const uint64_t end = sys::perf::HardwareCycleCounter::end_cycles();
            tracker.record_cycles(end - start);
        }

        int status;
        ::waitpid(pid, &status, 0);

        tracker.print_summary("Cross-Process SHM Ping-Pong Latency", std::cout);

        const auto stats = tracker.compute_stats();
        const double one_way_latency = stats.p50_ns / 2.0;

        std::cout << "------------------------------------------------------------------------\n"
                  << " Median One-Way IPC Latency: " << std::fixed << std::setprecision(2) 
                  << one_way_latency << " ns\n"
                  << "------------------------------------------------------------------------\n\n";

        if (one_way_latency < 500.0) {
            std::cout << ">>> VERIFIED: Achieved sub-500ns cross-process IPC latency! <<<\n";
        }
    }

    return 0;
}
