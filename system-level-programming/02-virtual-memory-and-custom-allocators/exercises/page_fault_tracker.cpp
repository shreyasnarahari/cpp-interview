#include "page_fault_tracker.hpp"
#include <iostream>
#include <vector>
#include <sys/mman.h>
#include <cstring>

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Linux Page Fault & Demand Paging Demonstration (C++20)\n";
    std::cout << "========================================================================\n\n";

    constexpr size_t PAGE_SIZE = 4096;
    constexpr size_t NUM_PAGES = 256; // 1 MB
    constexpr size_t TOTAL_SIZE = NUM_PAGES * PAGE_SIZE;

    std::cout << "Allocating " << (TOTAL_SIZE / 1024) << " KB (" << NUM_PAGES << " pages) via mmap...\n";

    void* ptr = nullptr;
    {
        sys::vm::PageFaultTracker::ScopedTracker tracker("Stage 1: mmap (MAP_ANONYMOUS | MAP_PRIVATE)");
        ptr = ::mmap(nullptr, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    }

    if (ptr == MAP_FAILED) {
        std::cerr << "mmap failed!\n";
        return 1;
    }

    auto* byte_ptr = static_cast<volatile uint8_t*>(ptr);

    // Stage 2: First touch (Demand Paging triggers minor page faults)
    {
        sys::vm::PageFaultTracker::ScopedTracker tracker("Stage 2: First Touch (Write 1 byte per page)");
        for (size_t i = 0; i < NUM_PAGES; ++i) {
            byte_ptr[i * PAGE_SIZE] = 0xAA;
        }
    }

    // Stage 3: Second touch (No page faults because PTE Present == 1)
    {
        sys::vm::PageFaultTracker::ScopedTracker tracker("Stage 3: Second Touch (Rewrite all pages)");
        for (size_t i = 0; i < NUM_PAGES; ++i) {
            byte_ptr[i * PAGE_SIZE] = 0xBB;
        }
    }

    // Stage 4: madvise MADV_DONTNEED (Drops physical page frames from page table)
    {
        sys::vm::PageFaultTracker::ScopedTracker tracker("Stage 4: madvise(MADV_DONTNEED)");
        ::madvise(ptr, TOTAL_SIZE, MADV_DONTNEED);
    }

    // Stage 5: Third touch after MADV_DONTNEED (Demand Paging re-triggers page faults)
    {
        sys::vm::PageFaultTracker::ScopedTracker tracker("Stage 5: Third Touch after MADV_DONTNEED");
        for (size_t i = 0; i < NUM_PAGES; ++i) {
            byte_ptr[i * PAGE_SIZE] = 0xCC;
        }
    }

    ::munmap(ptr, TOTAL_SIZE);
    std::cout << "\n========================================================================\n";
    std::cout << " Demonstration Completed Successfully\n";
    std::cout << "========================================================================\n";

    return 0;
}
