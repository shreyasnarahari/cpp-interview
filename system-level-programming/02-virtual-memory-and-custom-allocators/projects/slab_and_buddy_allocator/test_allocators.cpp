#include "buddy_allocator.hpp"
#include "slab_allocator.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>

struct MarketOrder {
    uint64_t order_id;
    double price;
    uint32_t quantity;
    bool is_buy;

    MarketOrder(uint64_t id, double p, uint32_t q, bool buy)
        : order_id(id), price(p), quantity(q), is_buy(buy) {
        ++constructor_calls;
    }

    ~MarketOrder() {
        ++destructor_calls;
    }

    static inline size_t constructor_calls{0};
    static inline size_t destructor_calls{0};
};

void test_buddy_allocator() {
    std::cout << "--- Testing Buddy Allocator ---\n";
    sys::alloc::BuddyAllocator buddy;

    std::cout << "  Initial Pool Size: " << (buddy.get_total_pool_size() / 1024) << " KB\n";
    assert(buddy.get_allocated_bytes() == 0);

    // Test 1: Allocate small 4KB blocks
    void* p1 = buddy.allocate(4096);
    void* p2 = buddy.allocate(4096);
    assert(p1 != nullptr && p2 != nullptr);
    assert(p1 != p2);
    assert(buddy.get_split_count() > 0);
    std::cout << "  Allocated two 4KB blocks. Splits performed: " << buddy.get_split_count() << "\n";

    // Test 2: Write data and verify
    std::memset(p1, 0xAA, 4096);
    std::memset(p2, 0xBB, 4096);
    assert(static_cast<uint8_t*>(p1)[0] == 0xAA);
    assert(static_cast<uint8_t*>(p2)[0] == 0xBB);

    // Test 3: Allocate a larger 64KB block
    void* p3 = buddy.allocate(64 * 1024);
    assert(p3 != nullptr);
    std::cout << "  Allocated 64KB block. Total allocated: " << (buddy.get_allocated_bytes() / 1024) << " KB\n";

    // Test 4: Deallocate and verify merging
    const uint64_t merges_before = buddy.get_merge_count();
    buddy.deallocate(p1, 4096);
    buddy.deallocate(p2, 4096);
    buddy.deallocate(p3, 64 * 1024);
    
    std::cout << "  Deallocated blocks. Merges performed: " << (buddy.get_merge_count() - merges_before) << "\n";
    assert(buddy.get_allocated_bytes() == 0);

    // Test 5: Full 1MB block re-allocation (proves complete coalescence)
    void* p_full = buddy.allocate(buddy.get_total_pool_size());
    assert(p_full != nullptr);
    std::cout << "  Successfully re-allocated entire 1MB block after coalescence!\n";
    buddy.deallocate(p_full, buddy.get_total_pool_size());
    assert(buddy.get_allocated_bytes() == 0);
}

void test_slab_allocator() {
    std::cout << "\n--- Testing Slab Allocator ---\n";
    MarketOrder::constructor_calls = 0;
    MarketOrder::destructor_calls = 0;

    {
        sys::alloc::SlabAllocator<MarketOrder, 128> slab;
        std::cout << "  Initial Capacity: " << slab.get_total_capacity() << " objects\n";

        std::vector<MarketOrder*> orders;
        orders.reserve(300);

        // Allocate 300 orders (will force slab to grow from 128 -> 256 -> 384)
        for (uint64_t i = 1; i <= 300; ++i) {
            MarketOrder* ord = slab.construct(i, 150.25 + static_cast<double>(i), 100, (i % 2 == 0));
            assert(ord != nullptr);
            assert(ord->order_id == i);
            orders.push_back(ord);
        }

        std::cout << "  Allocated 300 objects. Slab count: " << slab.get_slab_count() 
                  << " | Capacity: " << slab.get_total_capacity() << "\n";
        assert(slab.get_active_objects() == 300);
        assert(MarketOrder::constructor_calls == 300);
        assert(slab.get_slab_count() >= 3);

        // Destroy 100 orders
        for (size_t i = 0; i < 100; ++i) {
            slab.destroy(orders[i]);
        }
        assert(slab.get_active_objects() == 200);
        assert(MarketOrder::destructor_calls == 100);

        // Reallocate 100 orders (reusing the intrusive free-list)
        for (uint64_t i = 501; i <= 600; ++i) {
            MarketOrder* ord = slab.construct(i, 200.0, 50, true);
            assert(ord != nullptr);
        }
        assert(slab.get_active_objects() == 300);

        // Destroy remaining
        for (size_t i = 100; i < 300; ++i) {
            slab.destroy(orders[i]);
        }
    }

    std::cout << "  Slab Allocator verified successfully.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 02: Buddy and Slab Allocator Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_buddy_allocator();
    test_slab_allocator();

    std::cout << "\n>>> ALL ALLOCATOR TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
