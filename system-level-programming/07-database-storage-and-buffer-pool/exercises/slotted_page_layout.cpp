#include "slotted_page.hpp"
#include <iostream>

int main() {
    std::cout << "========================================================================\n";
    std::cout << " 4KB Slotted Page Storage Engine Demonstration (C++20)\n";
    std::cout << "========================================================================\n\n";

    db::storage::SlottedPage page(101);
    std::cout << "Initial Page ID: " << page.page_id() 
              << " | Free Space: " << page.free_space() << " bytes\n\n";

    uint16_t s0 = 0, s1 = 0, s2 = 0;
    page.insert_tuple("Tuple_0: User Shreyas (ID: 1001)", s0);
    page.insert_tuple("Tuple_1: Account Balance $150,000.50", s1);
    page.insert_tuple("Tuple_2: Limit Order BUY 500 AAPL @ $185.20", s2);

    std::cout << "Inserted 3 variable-length tuples:\n";
    for (uint16_t s : {s0, s1, s2}) {
        std::string payload;
        if (page.get_tuple(s, payload)) {
            std::cout << "  Slot " << s << ": " << payload << "\n";
        }
    }

    std::cout << "\nFree Space after inserts: " << page.free_space() << " bytes\n";

    // Delete middle tuple to create internal fragmentation
    page.delete_tuple(s1);
    std::cout << "Deleted Slot " << s1 << " (created fragmented hole).\n";

    page.compact();
    std::cout << "Compacted page. Free Space after defragmentation: " << page.free_space() << " bytes\n\n";

    std::cout << "Remaining valid tuples:\n";
    for (uint16_t s : {s0, s2}) {
        std::string payload;
        if (page.get_tuple(s, payload)) {
            std::cout << "  Slot " << s << ": " << payload << "\n";
        }
    }

    return 0;
}
