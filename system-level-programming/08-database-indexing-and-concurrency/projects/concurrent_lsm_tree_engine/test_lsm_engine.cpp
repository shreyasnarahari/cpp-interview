#include "lsm_engine.hpp"
#include <iostream>
#include <cassert>

void test_lsm_put_flush_get() {
    std::cout << "[Test 1] Testing LSM-Tree MemTable Flush & Multi-Level SSTable Lookups ...\n";
    const std::string db_dir = "/tmp/test_lsm_db";
    std::filesystem::remove_all(db_dir);

    {
        // Set small memtable threshold (4 KB) to trigger automatic flushes
        db::lsm::LsmEngine engine(db_dir, 4 * 1024);

        for (uint64_t i = 1; i <= 2000; ++i) {
            engine.put(i, i * 100);
        }

        std::cout << "  Flushed SSTables generated: " << engine.sstable_count() << "\n";
        assert(engine.sstable_count() > 0);

        // Verify all 2000 keys across SSTables and active MemTable
        for (uint64_t i = 1; i <= 2000; ++i) {
            auto val = engine.get(i);
            assert(val.has_value());
            assert(*val == i * 100);
        }

        // Test non-existent keys (testing Bloom filter rejection)
        assert(engine.get(999999) == std::nullopt);
        assert(engine.get(888888) == std::nullopt);
    }

    std::filesystem::remove_all(db_dir);
    std::cout << "  LSM-Tree flush, point lookups, and Bloom Filter tests passed.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 08: LSM-Tree Storage Engine Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_lsm_put_flush_get();

    std::cout << "\n>>> ALL LSM-TREE ENGINE TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
