#pragma once

#include "bloom_filter.hpp"
#include "skiplist_memtable.hpp"
#include "sstable.hpp"
#include <string>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <mutex>
#include <filesystem>
#include <iostream>

namespace db::lsm {

class LsmEngine {
public:
    explicit LsmEngine(std::string db_dir, size_t memtable_threshold_bytes = 64 * 1024)
        : db_dir_(std::move(db_dir)),
          memtable_threshold_bytes_(memtable_threshold_bytes),
          memtable_(std::make_unique<SkipListMemTable>()) {
        std::filesystem::create_directories(db_dir_);
    }

    ~LsmEngine() {
        flush_memtable();
    }

    LsmEngine(const LsmEngine&) = delete;
    LsmEngine& operator=(const LsmEngine&) = delete;

    void put(uint64_t key, uint64_t value) {
        std::unique_lock<std::shared_mutex> lock(engine_latch_);
        memtable_->put(key, value);

        if (memtable_->byte_size() >= memtable_threshold_bytes_) {
            flush_memtable_internal();
        }
    }

    [[nodiscard]] std::optional<uint64_t> get(uint64_t key) const {
        std::shared_lock<std::shared_mutex> lock(engine_latch_);

        // Step 1: Search active MemTable
        uint64_t val = 0;
        if (memtable_->get(key, val)) {
            return val;
        }

        // Step 2: Search SSTables in reverse chronological order (Level 0)
        for (auto it = sstables_.rbegin(); it != sstables_.rend(); ++it) {
            if ((*it)->get(key, val)) {
                return val;
            }
        }

        return std::nullopt;
    }

    void flush_memtable() {
        std::unique_lock<std::shared_mutex> lock(engine_latch_);
        flush_memtable_internal();
    }

    [[nodiscard]] size_t sstable_count() const noexcept {
        std::shared_lock<std::shared_mutex> lock(engine_latch_);
        return sstables_.size();
    }

private:
    void flush_memtable_internal() {
        if (memtable_->size() == 0) return;

        const std::string sst_path = db_dir_ + "/sstable_" + std::to_string(next_sst_id_++) + ".db";
        if (SSTable::build(sst_path, *memtable_)) {
            auto sst = SSTable::open(sst_path);
            if (sst) {
                sstables_.push_back(std::move(sst));
            }
        }

        memtable_ = std::make_unique<SkipListMemTable>();
    }

    std::string db_dir_;
    size_t memtable_threshold_bytes_;
    uint64_t next_sst_id_{0};
    std::unique_ptr<SkipListMemTable> memtable_;
    std::vector<std::unique_ptr<SSTable>> sstables_;
    mutable std::shared_mutex engine_latch_;
};

} // namespace db::lsm
