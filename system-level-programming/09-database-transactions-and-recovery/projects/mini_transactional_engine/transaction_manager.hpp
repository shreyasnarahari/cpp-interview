#pragma once

#include "../exercises/mvcc_undo_log.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <memory>

namespace db::tx {

enum class TxnState : uint8_t {
    ACTIVE = 0,
    COMMITTED = 1,
    ABORTED = 2
};

struct Transaction {
    txn_id_t txn_id{0};
    TxnState state{TxnState::ACTIVE};
    ReadView read_view;
    std::vector<std::pair<uint64_t, std::string>> undo_entries; // (key, old_value)
};

class TransactionManager {
public:
    TransactionManager() = default;

    std::shared_ptr<Transaction> begin_transaction() {
        std::unique_lock<std::shared_mutex> lock(tx_latch_);

        const txn_id_t tid = next_txn_id_++;
        auto txn = std::make_shared<Transaction>();
        txn->txn_id = tid;
        txn->state = TxnState::ACTIVE;

        // Build Snapshot ReadView
        txn->read_view.creator_txn_id = tid;
        txn->read_view.low_limit_id = tid;

        txn_id_t min_active = tid;
        std::vector<txn_id_t> active_list;
        for (const auto& [id, t] : active_txns_) {
            if (t->state == TxnState::ACTIVE) {
                active_list.push_back(id);
                if (id < min_active) {
                    min_active = id;
                }
            }
        }
        std::sort(active_list.begin(), active_list.end());

        txn->read_view.up_limit_id = min_active;
        txn->read_view.active_ids = std::move(active_list);

        active_txns_[tid] = txn;
        return txn;
    }

    void commit(std::shared_ptr<Transaction> txn) {
        std::unique_lock<std::shared_mutex> lock(tx_latch_);
        txn->state = TxnState::COMMITTED;
        active_txns_.erase(txn->txn_id);
    }

    void abort(std::shared_ptr<Transaction> txn) {
        std::unique_lock<std::shared_mutex> lock(tx_latch_);
        txn->state = TxnState::ABORTED;
        active_txns_.erase(txn->txn_id);
    }

    void set_next_txn_id(txn_id_t next_id) noexcept {
        std::unique_lock<std::shared_mutex> lock(tx_latch_);
        next_txn_id_.store(next_id);
    }

private:
    std::atomic<txn_id_t> next_txn_id_{1};
    std::unordered_map<txn_id_t, std::shared_ptr<Transaction>> active_txns_;
    mutable std::shared_mutex tx_latch_;
};

} // namespace db::tx
