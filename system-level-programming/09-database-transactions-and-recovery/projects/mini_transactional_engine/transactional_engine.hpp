#pragma once

#include "transaction_manager.hpp"
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <iostream>

namespace db::tx {

enum class WalRecordType : uint8_t {
    PUT = 1,
    COMMIT = 2,
    ABORT = 3
};

#pragma pack(push, 1)
struct WalHeader {
    WalRecordType type;
    txn_id_t txn_id;
    uint64_t key;
    uint32_t val_size;
};
#pragma pack(pop)

class TransactionalEngine {
public:
    explicit TransactionalEngine(std::string wal_path)
        : wal_path_(std::move(wal_path)) {
        wal_fd_ = ::open(wal_path_.c_str(), O_CREAT | O_RDWR | O_APPEND, 0644);
        if (wal_fd_ < 0) {
            throw std::runtime_error("Failed to open WAL file: " + wal_path_);
        }
    }

    ~TransactionalEngine() {
        if (wal_fd_ >= 0) {
            ::close(wal_fd_);
            wal_fd_ = -1;
        }
    }

    TransactionalEngine(const TransactionalEngine&) = delete;
    TransactionalEngine& operator=(const TransactionalEngine&) = delete;

    std::shared_ptr<Transaction> begin_transaction() {
        return tx_manager_.begin_transaction();
    }

    [[nodiscard]] std::optional<std::string> get(
        const std::shared_ptr<Transaction>& txn,
        uint64_t key
    ) {
        std::shared_lock<std::shared_mutex> lock(engine_latch_);

        auto it = table_.find(key);
        if (it == table_.end()) {
            return std::nullopt;
        }

        return read_tuple_snapshot(it->second, txn->read_view);
    }

    void put(
        const std::shared_ptr<Transaction>& txn,
        uint64_t key,
        std::string value
    ) {
        std::unique_lock<std::shared_mutex> lock(engine_latch_);

        // Write WAL entry before mutating in-memory state (WAL Protocol)
        WalHeader wal{};
        wal.type = WalRecordType::PUT;
        wal.txn_id = txn->txn_id;
        wal.key = key;
        wal.val_size = static_cast<uint32_t>(value.size());

        ssize_t w1 = ::write(wal_fd_, &wal, sizeof(WalHeader)); (void)w1;
        ssize_t w2 = ::write(wal_fd_, value.data(), value.size()); (void)w2;

        auto& slot = table_[key];
        slot.key = key;

        // Build Undo Record
        auto undo = std::make_shared<UndoRecord>();
        undo->xmin = slot.xmin;
        undo->old_val = slot.val;
        undo->prev = slot.roll_ptr;

        slot.roll_ptr = undo;
        slot.xmin = txn->txn_id;
        slot.val = std::move(value);

        txn->undo_entries.emplace_back(key, undo->old_val);
    }

    void commit(const std::shared_ptr<Transaction>& txn) {
        std::unique_lock<std::shared_mutex> lock(engine_latch_);

        WalHeader wal{};
        wal.type = WalRecordType::COMMIT;
        wal.txn_id = txn->txn_id;
        wal.key = 0;
        wal.val_size = 0;

        ssize_t w = ::write(wal_fd_, &wal, sizeof(WalHeader)); (void)w;
        ::fdatasync(wal_fd_); // Force WAL to non-volatile storage

        tx_manager_.commit(txn);
    }

    void abort(const std::shared_ptr<Transaction>& txn) {
        std::unique_lock<std::shared_mutex> lock(engine_latch_);

        // Roll back uncommitted changes
        for (const auto& [key, old_val] : txn->undo_entries) {
            auto it = table_.find(key);
            if (it != table_.end()) {
                if (it->second.roll_ptr) {
                    it->second.val = it->second.roll_ptr->old_val;
                    it->second.xmin = it->second.roll_ptr->xmin;
                    it->second.roll_ptr = it->second.roll_ptr->prev;
                }
            }
        }

        WalHeader wal{};
        wal.type = WalRecordType::ABORT;
        wal.txn_id = txn->txn_id;
        wal.key = 0;
        wal.val_size = 0;

        ssize_t w = ::write(wal_fd_, &wal, sizeof(WalHeader)); (void)w;
        ::fdatasync(wal_fd_);

        tx_manager_.abort(txn);
    }

    /**
     * @brief ARIES Crash Recovery: Two-pass Analysis and Redo Replay.
     */
    void recover_from_wal() {
        std::unique_lock<std::shared_mutex> lock(engine_latch_);
        table_.clear();

        int rfd = ::open(wal_path_.c_str(), O_RDONLY);
        if (rfd < 0) return;

        // Phase 1: Analysis Phase - Discover committed vs aborted transactions
        std::unordered_set<txn_id_t> committed_txns;
        txn_id_t max_txn_id = 0;
        WalHeader header{};
        while (::read(rfd, &header, sizeof(WalHeader)) == sizeof(WalHeader)) {
            if (header.txn_id > max_txn_id) {
                max_txn_id = header.txn_id;
            }
            if (header.type == WalRecordType::PUT) {
                ::lseek(rfd, header.val_size, SEEK_CUR); // Skip value payload
            } else if (header.type == WalRecordType::COMMIT) {
                committed_txns.insert(header.txn_id);
            }
        }

        // Advance Transaction Manager counter past all recovered transactions
        tx_manager_.set_next_txn_id(max_txn_id + 1);

        // Phase 2: Redo Phase - Sequential replay of committed writes
        ::lseek(rfd, 0, SEEK_SET);
        while (::read(rfd, &header, sizeof(WalHeader)) == sizeof(WalHeader)) {
            if (header.type == WalRecordType::PUT) {
                std::string val(header.val_size, '\0');
                ssize_t r = ::read(rfd, val.data(), header.val_size); (void)r;

                if (committed_txns.contains(header.txn_id)) {
                    auto& slot = table_[header.key];
                    slot.key = header.key;
                    slot.val = std::move(val);
                    slot.xmin = header.txn_id;
                }
            }
        }
        ::close(rfd);
    }

    [[nodiscard]] size_t size() const noexcept {
        std::shared_lock<std::shared_mutex> lock(engine_latch_);
        return table_.size();
    }

private:
    std::string wal_path_;
    int wal_fd_{-1};
    TransactionManager tx_manager_;
    std::unordered_map<uint64_t, TupleSlot> table_;
    mutable std::shared_mutex engine_latch_;
};

} // namespace db::tx
