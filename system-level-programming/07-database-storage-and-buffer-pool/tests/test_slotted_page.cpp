#include "../exercises/slotted_page.hpp"
#include <iostream>
#include <cassert>

void test_slotted_page_crud() {
    std::cout << "[Test 1] Testing Slotted Page CRUD Operations ...\n";
    db::storage::SlottedPage page(42);

    const uint16_t initial_free = page.free_space();
    (void)initial_free;

    uint16_t s0 = 0, s1 = 0, s2 = 0;
    assert(page.insert_tuple("Hello Slotted Page!", s0));
    assert(page.insert_tuple("Second Database Record with Numbers 1234567890", s1));
    assert(page.insert_tuple("Third Small Record", s2));

    std::string p0, p1, p2;
    assert(page.get_tuple(s0, p0) && p0 == "Hello Slotted Page!");
    assert(page.get_tuple(s1, p1) && p1 == "Second Database Record with Numbers 1234567890");
    assert(page.get_tuple(s2, p2) && p2 == "Third Small Record");

    // Update with larger payload
    assert(page.update_tuple(s0, "Hello Slotted Page! Updated and Expanded to Larger String"));
    assert(page.get_tuple(s0, p0) && p0 == "Hello Slotted Page! Updated and Expanded to Larger String");

    // Delete s1
    assert(page.delete_tuple(s1));
    assert(!page.get_tuple(s1, p1));

    // Compact
    page.compact();

    // Verify surviving tuples
    assert(page.get_tuple(s0, p0) && p0 == "Hello Slotted Page! Updated and Expanded to Larger String");
    assert(page.get_tuple(s2, p2) && p2 == "Third Small Record");

    // Insert into reused slot
    uint16_t s3 = 0;
    assert(page.insert_tuple("Reused Slot Record", s3));
    assert(s3 == s1); // Reused deleted slot index 1

    std::cout << "  Slotted page CRUD, updates, slot reuse and compaction verified.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 07: Slotted Page Storage Tests\n";
    std::cout << "=======================================================\n\n";

    test_slotted_page_crud();

    std::cout << "\n>>> ALL SLOTTED PAGE TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
