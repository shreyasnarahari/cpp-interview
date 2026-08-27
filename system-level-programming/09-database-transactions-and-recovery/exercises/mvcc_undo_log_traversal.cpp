#include "mvcc_undo_log.hpp"
#include <iostream>

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Multi-Version Concurrency Control (MVCC) Version Traversal (C++20)\n";
    std::cout << "========================================================================\n\n";

    // Scenario:
    // Txn 100 inserts Key 1 -> "AAPL @ $150.00" (Committed)
    // Txn 200 updates Key 1 -> "AAPL @ $175.00" (Committed)
    // Txn 300 starts a ReadView (Snapshot at Txn 200)
    // Txn 400 updates Key 1 -> "AAPL @ $200.00" (Committed)
    // Txn 500 updates Key 1 -> "AAPL @ $225.00" (Uncommitted)

    db::tx::TupleSlot tuple;
    tuple.key = 1;
    tuple.val = "AAPL @ $225.00";
    tuple.xmin = 500;

    auto undo400 = std::make_shared<db::tx::UndoRecord>();
    undo400->xmin = 400;
    undo400->old_val = "AAPL @ $200.00";

    auto undo200 = std::make_shared<db::tx::UndoRecord>();
    undo200->xmin = 200;
    undo200->old_val = "AAPL @ $175.00";

    auto undo100 = std::make_shared<db::tx::UndoRecord>();
    undo100->xmin = 100;
    undo100->old_val = "AAPL @ $150.00";
    undo100->prev = nullptr;

    undo200->prev = undo100;
    undo400->prev = undo200;
    tuple.roll_ptr = undo400;

    // Construct ReadView for Txn 300
    db::tx::ReadView view300;
    view300.creator_txn_id = 300;
    view300.up_limit_id = 250;  // All txns < 250 were committed
    view300.low_limit_id = 301; // Txns >= 301 were not started yet
    view300.active_ids = {};    // No uncommitted txns < 250

    auto snapshot_val = db::tx::read_tuple_snapshot(tuple, view300);

    std::cout << "Current Heap Row (Txn 500): " << tuple.val << "\n";
    if (snapshot_val.has_value()) {
        std::cout << "Txn 300 ReadView Snapshot:  " << *snapshot_val << " (Traversed Undo Log)\n";
    }

    std::cout << "\nMVCC Undo Log version chain traversal completed successfully.\n";
    return 0;
}
