#include "transactional_engine.hpp"
#include <iostream>
#include <cassert>

void test_acid_transactions() {
    std::cout << "[Test 1] Testing ACID Transaction Commit, Abort, and Snapshot Isolation ...\n";
    const std::string wal_file = "/tmp/test_mini_tx.wal";
    ::unlink(wal_file.c_str());

    {
        db::tx::TransactionalEngine engine(wal_file);

        // Txn 1: Inserts Key 10 -> "Apple" and Key 20 -> "Google" (Commits)
        auto tx1 = engine.begin_transaction();
        engine.put(tx1, 10, "Apple");
        engine.put(tx1, 20, "Google");
        engine.commit(tx1);

        // Txn 2: Starts snapshot read view
        auto tx2 = engine.begin_transaction();

        // Txn 3: Updates Key 10 -> "Apple Inc" and Aborts
        auto tx3 = engine.begin_transaction();
        engine.put(tx3, 10, "Apple Inc");
        engine.abort(tx3); // Rollback

        // Txn 2 reads Key 10 -> should still be "Apple"
        auto val10 = engine.get(tx2, 10);
        assert(val10.has_value() && *val10 == "Apple");

        // Txn 4: Updates Key 20 -> "Alphabet" (Commits)
        auto tx4 = engine.begin_transaction();
        engine.put(tx4, 20, "Alphabet");
        engine.commit(tx4);

        // Txn 2 reads Key 20 under Snapshot Isolation -> should still see "Google"!
        auto val20 = engine.get(tx2, 20);
        assert(val20.has_value() && *val20 == "Google");

        // Txn 5: New transaction after Txn 4 commit -> should see "Alphabet"
        auto tx5 = engine.begin_transaction();
        auto val20_new = engine.get(tx5, 20);
        assert(val20_new.has_value() && *val20_new == "Alphabet");

        engine.commit(tx2);
        engine.commit(tx5);
    }

    std::cout << "  ACID Commit, Abort rollback, and Snapshot Isolation passed.\n";

    // Test 2: WAL Crash Recovery Replay
    std::cout << "[Test 2] Testing ARIES WAL Crash Recovery Replay ...\n";
    {
        db::tx::TransactionalEngine engine(wal_file);
        engine.recover_from_wal();

        auto tx_rec = engine.begin_transaction();
        auto val10 = engine.get(tx_rec, 10);
        auto val20 = engine.get(tx_rec, 20);

        assert(val10.has_value() && *val10 == "Apple");
        assert(val20.has_value() && *val20 == "Alphabet"); // Committed write restored!
        engine.commit(tx_rec);
    }

    ::unlink(wal_file.c_str());
    std::cout << "  Crash recovery replay restored 100% of committed transactions.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 09: Transaction Engine Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_acid_transactions();

    std::cout << "\n>>> ALL TRANSACTION ENGINE TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
