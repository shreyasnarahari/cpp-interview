#include "../exercises/latch_crabbing_btree.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

void test_concurrent_btree() {
    std::cout << "[Test 1] Testing Multi-Threaded B+ Tree Latch Crabbing ...\n";
    db::index::ConcurrentBTree<uint64_t, uint64_t, 16> tree;

    constexpr size_t NUM_WRITERS = 4;
    constexpr size_t KEYS_PER_WRITER = 500;
    constexpr size_t TOTAL_KEYS = NUM_WRITERS * KEYS_PER_WRITER;

    std::vector<std::thread> writers;
    writers.reserve(NUM_WRITERS);

    for (size_t w = 0; w < NUM_WRITERS; ++w) {
        writers.emplace_back([&, w] {
            const uint64_t base = w * KEYS_PER_WRITER;
            for (uint64_t i = 1; i <= KEYS_PER_WRITER; ++i) {
                const uint64_t key = base + i;
                tree.insert(key, key * 10);
            }
        });
    }

    for (auto& t : writers) {
        t.join();
    }

    std::cout << "  All " << TOTAL_KEYS << " keys inserted concurrently.\n";

    // Multi-threaded concurrent reads
    constexpr size_t NUM_READERS = 8;
    std::vector<std::thread> readers;
    readers.reserve(NUM_READERS);

    for (size_t r = 0; r < NUM_READERS; ++r) {
        readers.emplace_back([&] {
            for (uint64_t k = 1; k <= TOTAL_KEYS; ++k) {
                auto val = tree.search(k);
                assert(val.has_value());
                assert(*val == k * 10);
            }
        });
    }

    for (auto& t : readers) {
        t.join();
    }

    std::cout << "  Concurrent read queries validated all " << TOTAL_KEYS << " key-value mappings.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 08: B+ Tree Latch Crabbing Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_concurrent_btree();

    std::cout << "\n>>> ALL B+ TREE LATCH CRABBING TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
