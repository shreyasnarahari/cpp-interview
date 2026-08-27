#include "../exercises/memory_order_drills.hpp"
#include <iostream>
#include <thread>
#include <cassert>
#include <vector>

void test_acquire_release_synchronization() {
    std::cout << "[Test 1] Testing Multi-Threaded Acquire-Release Synchronization ...\n";
    constexpr size_t MESSAGES = 100'000;

    sys::sync::AcquireReleaseChannel channel;

    std::thread producer([&] {
        for (uint64_t i = 1; i <= MESSAGES; ++i) {
            while (!channel.try_send(i, 100.5 + static_cast<double>(i), static_cast<uint32_t>(i * 10))) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        size_t received = 0;
        while (received < MESSAGES) {
            sys::sync::OrderData data;
            if (channel.try_receive(data)) {
                ++received;
                assert(data.valid == true);
                assert(data.order_id == received);
                assert(data.qty == received * 10);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    std::cout << "  Successfully synchronized " << MESSAGES << " messages without mutexes.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 04: Memory Order Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_acquire_release_synchronization();

    std::cout << "\n>>> ALL MEMORY ORDER TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
