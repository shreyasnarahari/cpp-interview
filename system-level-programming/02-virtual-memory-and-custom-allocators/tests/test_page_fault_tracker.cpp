#include "../exercises/page_fault_tracker.hpp"
#include <iostream>
#include <cassert>
#include <sys/mman.h>

void test_page_fault_counting() {
    std::cout << "[Test 1] Testing minor page fault triggering ...\n";
    constexpr size_t PAGE_SIZE = 4096;
    constexpr size_t NUM_PAGES = 100;
    constexpr size_t TOTAL_SIZE = NUM_PAGES * PAGE_SIZE;

    const auto before_map = sys::vm::PageFaultTracker::get_current_stats();
    void* ptr = ::mmap(nullptr, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(ptr != MAP_FAILED);
    const auto after_map = sys::vm::PageFaultTracker::get_current_stats();

    // mmap itself does NOT fault in anonymous private pages
    std::cout << "  Minor faults from mmap: " << (after_map.minor_faults - before_map.minor_faults) << "\n";

    // Touching pages should trigger demand faults
    auto* byte_ptr = static_cast<volatile uint8_t*>(ptr);
    const auto before_touch = sys::vm::PageFaultTracker::get_current_stats();
    for (size_t i = 0; i < NUM_PAGES; ++i) {
        byte_ptr[i * PAGE_SIZE] = static_cast<uint8_t>(i & 0xFF);
    }
    const auto after_touch = sys::vm::PageFaultTracker::get_current_stats();

    const uint64_t touch_faults = after_touch.minor_faults - before_touch.minor_faults;
    std::cout << "  Minor faults from first touch: " << touch_faults << " (expected >= " << NUM_PAGES << ")\n";
    assert(touch_faults >= NUM_PAGES);

    // Re-touching should trigger NO faults
    const auto before_retouch = sys::vm::PageFaultTracker::get_current_stats();
    for (size_t i = 0; i < NUM_PAGES; ++i) {
        byte_ptr[i * PAGE_SIZE] = static_cast<uint8_t>((i + 1) & 0xFF);
    }
    const auto after_retouch = sys::vm::PageFaultTracker::get_current_stats();
    const uint64_t retouch_faults = after_retouch.minor_faults - before_retouch.minor_faults;
    std::cout << "  Minor faults from retouch: " << retouch_faults << " (expected 0)\n";
    assert(retouch_faults == 0);

    ::munmap(ptr, TOTAL_SIZE);
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 02: Page Fault Tracker Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_page_fault_counting();

    std::cout << "\n>>> ALL PAGE FAULT TRACKER TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
