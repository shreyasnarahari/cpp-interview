#include "latch_crabbing_btree.hpp"
#include <iostream>

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Concurrent B+ Tree Index with Latch Crabbing Protocol (C++20)\n";
    std::cout << "========================================================================\n\n";

    db::index::ConcurrentBTree<uint64_t, uint64_t, 8> tree;

    std::cout << "Inserting 50 keys with Root-to-Leaf Write Latch Crabbing...\n";
    for (uint64_t i = 1; i <= 50; ++i) {
        tree.insert(i * 10, i * 100);
    }

    std::cout << "Performing Point Lookups with Read Latch Crabbing...\n";
    for (uint64_t i = 1; i <= 50; ++i) {
        auto val = tree.search(i * 10);
        if (i % 10 == 0 && val.has_value()) {
            std::cout << "  Key: " << (i * 10) << " -> Value: " << *val << "\n";
        }
    }

    std::cout << "\nConcurrent B+ Tree demonstration successful.\n";
    return 0;
}
