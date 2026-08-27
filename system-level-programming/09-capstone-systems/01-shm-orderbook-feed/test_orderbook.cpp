#include "orderbook.hpp"
#include "shm_feed.hpp"
#include <iostream>
#include <cassert>

void test_orderbook_depth() {
    std::cout << "[Test 1] Testing L2 Order Book Depth & Level Aggregation ...\n";
    sys::hft::L2OrderBook book;

    // Add bids
    book.update_level(sys::hft::Side::BUY, 100.0, 50, 2);
    book.update_level(sys::hft::Side::BUY, 100.5, 100, 5);
    book.update_level(sys::hft::Side::BUY, 99.5, 200, 10);

    // Add asks
    book.update_level(sys::hft::Side::SELL, 101.0, 75, 3);
    book.update_level(sys::hft::Side::SELL, 102.0, 150, 4);

    assert(book.bid_depth() == 3);
    assert(book.ask_depth() == 2);

    auto best_b = book.best_bid();
    assert(best_b.has_value());
    assert(best_b->price == 100.5);
    assert(best_b->qty == 100);

    auto best_a = book.best_ask();
    assert(best_a.has_value());
    assert(best_a->price == 101.0);
    assert(best_a->qty == 75);

    assert(book.mid_price() == 100.75);
    assert(book.spread() == 0.5);

    // Delete best bid (qty = 0)
    book.update_level(sys::hft::Side::BUY, 100.5, 0, 0);
    assert(book.bid_depth() == 2);
    assert(book.best_bid()->price == 100.0);

    std::cout << "  Orderbook matching & depth assertions passed.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Capstone 01: L2 Limit Order Book Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_orderbook_depth();

    std::cout << "\n>>> ALL ORDERBOOK TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
