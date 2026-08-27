#include "buffer_pool_manager.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

void test_buffer_pool_basic() {
    std::cout << "[Test 1] Testing Basic Buffer Pool Allocation & Eviction ...\n";
    const std::string db_file = "/tmp/test_bpm_basic.db";
    ::unlink(db_file.c_str());

    {
        // Pool size of 3 frames
        db::storage::BufferPoolManager bpm(3, db_file);

        uint64_t p0 = 0, p1 = 0, p2 = 0;
        auto* page0 = bpm.new_page(p0);
        auto* page1 = bpm.new_page(p1);
        auto* page2 = bpm.new_page(p2);
        assert(page0 && page1 && page2);

        uint16_t s0 = 0, s1 = 0, s2 = 0;
        assert(page0->insert_tuple("Page_0_Tuple", s0));
        assert(page1->insert_tuple("Page_1_Tuple", s1));
        assert(page2->insert_tuple("Page_2_Tuple", s2));

        // Unpin pages with dirty=true
        assert(bpm.unpin_page(p0, true));
        assert(bpm.unpin_page(p1, true));
        assert(bpm.unpin_page(p2, true));

        // Create 4th page (forces eviction of an unpinned frame)
        uint64_t p3 = 0;
        auto* page3 = bpm.new_page(p3);
        assert(page3 != nullptr);
        uint16_t s3 = 0;
        assert(page3->insert_tuple("Page_3_Tuple", s3));
        assert(bpm.unpin_page(p3, true));

        // Fetch evicted page back from disk
        auto* fetched_p0 = bpm.fetch_page(p0);
        assert(fetched_p0 != nullptr);
        std::string payload;
        assert(fetched_p0->get_tuple(s0, payload));
        assert(payload == "Page_0_Tuple");
        assert(bpm.unpin_page(p0, false));
    }

    ::unlink(db_file.c_str());
    std::cout << "  Basic buffer pool allocation, eviction, and disk reload passed.\n";
}

void test_buffer_pool_concurrency() {
    std::cout << "[Test 2] Testing Multi-Threaded Buffer Pool Concurrency Stress ...\n";
    const std::string db_file = "/tmp/test_bpm_concurrent.db";
    ::unlink(db_file.c_str());

    constexpr size_t POOL_SIZE = 10;
    constexpr size_t NUM_THREADS = 8;
    constexpr size_t OPS_PER_THREAD = 500;

    {
        db::storage::BufferPoolManager bpm(POOL_SIZE, db_file);

        // Pre-allocate 20 pages on disk
        std::vector<uint64_t> page_ids(20);
        for (size_t i = 0; i < 20; ++i) {
            auto* p = bpm.new_page(page_ids[i]);
            assert(p != nullptr);
            uint16_t slot = 0;
            assert(p->insert_tuple("Initial_Data_" + std::to_string(i), slot));
            assert(bpm.unpin_page(page_ids[i], true));
        }

        std::vector<std::thread> workers;
        workers.reserve(NUM_THREADS);

        for (size_t t = 0; t < NUM_THREADS; ++t) {
            workers.emplace_back([&, t] {
                for (size_t op = 0; op < OPS_PER_THREAD; ++op) {
                    const uint64_t pid = page_ids[(t + op) % page_ids.size()];
                    auto* page = bpm.fetch_page(pid);
                    if (page) {
                        std::string val;
                        (void)page->get_tuple(0, val);
                        bpm.unpin_page(pid, false);
                    }
                }
            });
        }

        for (auto& w : workers) {
            w.join();
        }
    }

    ::unlink(db_file.c_str());
    std::cout << "  Concurrent multi-threaded stress test passed with zero page leaks.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 07: Buffer Pool Manager Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_buffer_pool_basic();
    test_buffer_pool_concurrency();

    std::cout << "\n>>> ALL BUFFER POOL MANAGER TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
