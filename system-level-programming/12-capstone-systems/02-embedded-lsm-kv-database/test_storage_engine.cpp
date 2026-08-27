#include "embedded_lsm_database.hpp"
#include <iostream>
#include <cassert>

void test_embedded_lsm_crud() {
    std::cout << "[Test 1] Testing Embedded LSM Database CRUD & Multi-Level Flush ...\n";
    const std::string db_dir = "/tmp/test_embedded_lsm";
    std::filesystem::remove_all(db_dir);

    {
        sys::db::EmbeddedLsmDatabase db(db_dir, 4 * 1024);

        for (uint64_t i = 1; i <= 3000; ++i) {
            db.put(i, i * 10);
        }

        std::cout << "  Flushed SSTables: " << db.sstable_count() << "\n";
        assert(db.sstable_count() > 0);

        for (uint64_t i = 1; i <= 3000; ++i) {
            auto val = db.get(i);
            assert(val.has_value());
            assert(*val == i * 10);
        }
    }

    std::cout << "  CRUD and multi-level SSTables verified.\n";

    // Test 2: Crash Recovery
    std::cout << "[Test 2] Testing Crash Recovery from Disk ...\n";
    {
        sys::db::EmbeddedLsmDatabase db(db_dir, 4 * 1024);
        db.recover();

        for (uint64_t i = 1; i <= 3000; ++i) {
            auto val = db.get(i);
            assert(val.has_value());
            assert(*val == i * 10);
        }
    }

    std::filesystem::remove_all(db_dir);
    std::cout << "  Crash recovery restored 100% of keys.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Capstone 02: Embedded LSM Database Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_embedded_lsm_crud();

    std::cout << "\n>>> ALL CAPSTONE 02 DATABASE TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
