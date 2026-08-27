#include "wal_storage_engine.hpp"
#include <iostream>
#include <cassert>

void test_wal_put_get_del() {
    std::cout << "[Test 1] Testing WAL Storage Put, Get, and Delete Operations ...\n";
    const std::string wal_file = "/tmp/test_wal_ops.db";
    ::unlink(wal_file.c_str());

    {
        sys::storage::WalStorageEngine engine(wal_file);
        assert(engine.open());

        assert(engine.put(101, 5555));
        assert(engine.put(102, 6666));
        assert(engine.put(103, 7777));

        assert(engine.get(101) == 5555);
        assert(engine.get(102) == 6666);
        assert(engine.get(103) == 7777);
        assert(engine.get(999) == std::nullopt);

        assert(engine.del(102));
        assert(engine.get(102) == std::nullopt);
        assert(engine.size() == 2);
    }

    std::cout << "  Put/Get/Del operations verified.\n";
}

void test_wal_crash_recovery() {
    std::cout << "[Test 2] Testing WAL Crash Recovery from Disk ...\n";
    const std::string wal_file = "/tmp/test_wal_crash.db";
    ::unlink(wal_file.c_str());

    // Phase 1: Write entries and close unexpectedly (simulate shutdown)
    {
        sys::storage::WalStorageEngine engine(wal_file);
        assert(engine.open());

        for (uint64_t i = 1; i <= 1000; ++i) {
            assert(engine.put(i, i * 10));
        }
        // Delete half of them
        for (uint64_t i = 1; i <= 500; ++i) {
            assert(engine.del(i));
        }
    }

    // Phase 2: Start a brand new engine instance pointing to the same file
    {
        sys::storage::WalStorageEngine recovered_engine(wal_file);
        assert(recovered_engine.open());

        std::cout << "  Recovered engine size: " << recovered_engine.size() << " (Expected: 500)\n";
        assert(recovered_engine.size() == 500);

        // Verify deleted keys are gone
        for (uint64_t i = 1; i <= 500; ++i) {
            assert(recovered_engine.get(i) == std::nullopt);
        }
        // Verify remaining keys have exact values
        for (uint64_t i = 501; i <= 1000; ++i) {
            assert(recovered_engine.get(i) == i * 10);
        }
    }

    ::unlink(wal_file.c_str());
    std::cout << "  Crash recovery successfully replayed all 1000 transactions.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Capstone 02: WAL Storage Engine Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_wal_put_get_del();
    test_wal_crash_recovery();

    std::cout << "\n>>> ALL WAL STORAGE ENGINE TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
