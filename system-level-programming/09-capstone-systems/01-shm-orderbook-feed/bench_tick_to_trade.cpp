#include "orderbook.hpp"
#include "shm_feed.hpp"
#include "../../common/rdtsc_timer.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cassert>
#include <immintrin.h>

int main() {
    constexpr size_t TICKS = 100'000;
    const std::string shm_name = "/shm_capstone_ob";

    std::cout << "========================================================================\n";
    std::cout << " Capstone 01: End-to-End Tick-to-Trade Processing Latency (" << TICKS << " Ticks)\n";
    std::cout << "========================================================================\n\n";

    sys::perf::TscCalibrator::instance().calibrate(20);

    auto feed_creator = sys::hft::ShmMarketFeed::create(shm_name, 65536);

    pid_t pid = ::fork();
    assert(pid >= 0);

    if (pid == 0) {
        // Child Process: Trading Engine (Consumer)
        auto feed_consumer = sys::hft::ShmMarketFeed::attach(shm_name, 65536);
        sys::hft::L2OrderBook book;
        sys::perf::LatencyTracker tracker(TICKS);

        size_t processed = 0;
        while (processed < TICKS) {
            sys::hft::MarketEvent event;
            if (feed_consumer.consume(event)) {
                // Update local L2 book
                book.update_level(event.side, event.price, event.qty, event.order_count);

                // Compute trading signal (e.g. mid-price change)
                const double mid = book.mid_price();
                (void)mid;

                const uint64_t now_cycles = sys::perf::HardwareCycleCounter::end_cycles();
                if (now_cycles > event.timestamp_ns) {
                    tracker.record_cycles(now_cycles - event.timestamp_ns);
                }
                ++processed;
            } else {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
        }

        tracker.print_summary("End-to-End Tick-to-Trade Latency", std::cout);

        const auto stats = tracker.compute_stats();
        if (stats.p50_ns < 1000.0) {
            std::cout << ">>> VERIFIED: Achieved SUB-MICROSECOND Tick-to-Trade Latency (" 
                      << stats.p50_ns << " ns P50)! <<<\n\n";
        }
        ::_exit(0);
    } else {
        // Parent Process: Exchange Simulator (Producer)
        for (uint64_t i = 1; i <= TICKS; ++i) {
            const double price = 100.0 + static_cast<double>(i % 50) * 0.1;
            const uint32_t qty = static_cast<uint32_t>(10 + (i % 100));
            const auto side = (i % 2 == 0) ? sys::hft::Side::BUY : sys::hft::Side::SELL;

            sys::hft::MarketEvent ev{};
            ev.seq_num = i;
            ev.price = price;
            ev.qty = qty;
            ev.order_count = 1;
            ev.side = side;
            ev.event_type = sys::hft::MarketEventType::UPDATE_LEVEL;
            ev.timestamp_ns = sys::perf::HardwareCycleCounter::start_cycles();

            while (!feed_creator.publish(ev)) {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
        }

        int status;
        ::waitpid(pid, &status, 0);
    }

    return 0;
}
