#include "../exercises/mvcc_undo_log.hpp"
#include <iostream>
#include <cassert>

void test_mvcc_isolation() {
    std::cout << "[Test 1] Testing MVCC Snapshot Isolation & Anomaly Prevention ...\n";

    // Setup a row:
    // Inserted by Txn 10 (Committed) -> "v10"
    // Updated by Txn 20 (Committed) -> "v20"
    // Updated by Txn 30 (Committed) -> "v30"
    // Updated by Txn 50 (Uncommitted / Active) -> "v50"

    db::tx::TupleSlot tuple;
    tuple.key = 42;
    tuple.val = "v50";
    tuple.xmin = 50;

    auto undo30 = std::make_shared<db::tx::UndoRecord>();
    undo30->xmin = 30;
    undo30->old_val = "v30";

    auto undo20 = std::make_shared<db::tx::UndoRecord>();
    undo20->xmin = 20;
    undo20->old_val = "v20";

    auto undo10 = std::make_shared<db::tx::UndoRecord>();
    undo10->xmin = 10;
    undo10->old_val = "v10";
    undo10->prev = nullptr;

    undo20->prev = undo10;
    undo30->prev = undo20;
    tuple.roll_ptr = undo30;

    // Case 1: Own transaction reads (Txn 50 reads own uncommitted write)
    db::tx::ReadView view50;
    view50.creator_txn_id = 50;
    view50.up_limit_id = 50;
    view50.low_limit_id = 51;
    view50.active_ids = {};
    auto val50 = db::tx::read_tuple_snapshot(tuple, view50);
    assert(val50.has_value() && *val50 == "v50");

    // Case 2: Snapshot taken at Txn 25 (Txn 10 and 20 committed, Txn 30 not started)
    // Should NOT see v50 (uncommitted), nor v30 (committed after). Must see v20!
    db::tx::ReadView view25;
    view25.creator_txn_id = 25;
    view25.up_limit_id = 21;
    view25.low_limit_id = 26;
    view25.active_ids = {};
    auto val25 = db::tx::read_tuple_snapshot(tuple, view25);
    assert(val25.has_value() && *val25 == "v20");

    // Case 3: Snapshot taken when Txn 20 was uncommitted / active
    db::tx::ReadView view_active20;
    view_active20.creator_txn_id = 22;
    view_active20.up_limit_id = 15;
    view_active20.low_limit_id = 23;
    view_active20.active_ids = {20}; // Txn 20 was still running
    auto val_act20 = db::tx::read_tuple_snapshot(tuple, view_active20);
    assert(val_act20.has_value() && *val_act20 == "v10"); // Must walk past Txn 20 to Txn 10!

    std::cout << "  MVCC Snapshot Isolation, Dirty Read & Non-Repeatable Read prevention verified.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 09: MVCC Snapshot Visibility Verification\n";
    std::cout << "=======================================================\n\n";

    test_mvcc_isolation();

    std::cout << "\n>>> ALL MVCC VISIBILITY TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
