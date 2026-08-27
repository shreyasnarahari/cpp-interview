#include "spsc_queue.hpp"
#include "mpmc_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <numeric>

void test_spsc_queue() {
    std::cout << "[Test 1] Testing Wait-Free SPSC Queue Multi-Threaded Stress ...\n";
    constexpr size_t TOTAL_ITEMS = 1'000'000;
    sys::lockfree::SPSCQueue<uint64_t, 32768> queue;

    uint64_t producer_sum = 0;
    std::thread producer([&] {
        for (uint64_t i = 1; i <= TOTAL_ITEMS; ++i) {
            producer_sum += i;
            while (!queue.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    uint64_t consumer_sum = 0;
    size_t items_received = 0;
    std::thread consumer([&] {
        uint64_t prev = 0;
        while (items_received < TOTAL_ITEMS) {
            uint64_t val = 0;
            if (queue.pop(val)) {
                assert(val == prev + 1); // Verify strict FIFO ordering
                prev = val;
                consumer_sum += val;
                ++items_received;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    std::cout << "  SPSC received: " << items_received << " items | Sum: " << consumer_sum << "\n";
    assert(items_received == TOTAL_ITEMS);
    assert(consumer_sum == producer_sum);
}

void test_mpmc_queue() {
    std::cout << "[Test 2] Testing Lock-Free MPMC Queue (4 Producers, 4 Consumers) ...\n";
    constexpr size_t PRODUCERS = 4;
    constexpr size_t CONSUMERS = 4;
    constexpr size_t ITEMS_PER_PRODUCER = 250'000;
    constexpr size_t TOTAL_ITEMS = PRODUCERS * ITEMS_PER_PRODUCER;

    sys::lockfree::MPMCQueue<uint64_t> queue(65536);

    std::atomic<uint64_t> total_produced_sum{0};
    std::atomic<uint64_t> total_consumed_sum{0};
    std::atomic<size_t> total_consumed_count{0};

    std::vector<std::thread> producers;
    producers.reserve(PRODUCERS);

    for (size_t p = 0; p < PRODUCERS; ++p) {
        producers.emplace_back([&, p] {
            uint64_t local_sum = 0;
            const uint64_t base = p * ITEMS_PER_PRODUCER;
            for (uint64_t i = 1; i <= ITEMS_PER_PRODUCER; ++i) {
                const uint64_t val = base + i;
                local_sum += val;
                while (!queue.push(val)) {
                    std::this_thread::yield();
                }
            }
            total_produced_sum.fetch_add(local_sum, std::memory_order_relaxed);
        });
    }

    std::vector<std::thread> consumers;
    consumers.reserve(CONSUMERS);

    for (size_t c = 0; c < CONSUMERS; ++c) {
        consumers.emplace_back([&] {
            uint64_t local_sum = 0;
            size_t local_count = 0;
            while (total_consumed_count.load(std::memory_order_relaxed) < TOTAL_ITEMS) {
                uint64_t val = 0;
                if (queue.pop(val)) {
                    local_sum += val;
                    ++local_count;
                    total_consumed_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
            total_consumed_sum.fetch_add(local_sum, std::memory_order_relaxed);
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    std::cout << "  MPMC Consumed " << total_consumed_count.load() 
              << " items | Sum: " << total_consumed_sum.load() << "\n";
    assert(total_consumed_count.load() == TOTAL_ITEMS);
    assert(total_consumed_sum.load() == total_produced_sum.load());
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 04: Lock-Free Queue Stress Tests\n";
    std::cout << "=======================================================\n\n";

    test_spsc_queue();
    test_mpmc_queue();

    std::cout << "\n>>> ALL LOCK-FREE QUEUE TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
