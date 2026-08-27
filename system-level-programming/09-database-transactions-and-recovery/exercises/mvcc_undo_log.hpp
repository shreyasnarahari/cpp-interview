#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <optional>
#include <iostream>

namespace db::tx {

using txn_id_t = uint64_t;

/**
 * @brief Active Transaction Snapshot (ReadView) for MVCC Visibility.
 */
struct ReadView {
    txn_id_t creator_txn_id{0};
    txn_id_t up_limit_id{0};   // Lowest active transaction ID at snapshot creation
    txn_id_t low_limit_id{0};  // Highest committed transaction ID + 1 at snapshot creation
    std::vector<txn_id_t> active_ids; // Set of active uncommitted transactions

    [[nodiscard]] bool is_visible(txn_id_t xmin) const noexcept {
        if (xmin == creator_txn_id) {
            return true; // Own modifications are always visible
        }
        if (xmin < up_limit_id) {
            return true; // Committed before this ReadView was created
        }
        if (xmin >= low_limit_id) {
            return false; // Created after this ReadView began
        }
        // Check if xmin was in active uncommitted set
        return !std::binary_search(active_ids.begin(), active_ids.end(), xmin);
    }
};

/**
 * @brief Historical Undo Log Record for Rollback and Version Traversal.
 */
struct UndoRecord {
    txn_id_t xmin{0};
    std::string old_val;
    std::shared_ptr<UndoRecord> prev{nullptr};
};

/**
 * @brief Current Table Tuple with MVCC Version Header.
 */
struct TupleSlot {
    uint64_t key{0};
    std::string val;
    txn_id_t xmin{0};
    std::shared_ptr<UndoRecord> roll_ptr{nullptr};
};

/**
 * @brief Reconstructs tuple snapshot by traversing the MVCC Undo Log chain.
 */
[[nodiscard]] inline std::optional<std::string> read_tuple_snapshot(
    const TupleSlot& tuple,
    const ReadView& view
) {
    if (view.is_visible(tuple.xmin)) {
        return tuple.val;
    }

    // Traverse historical version chain
    auto curr = tuple.roll_ptr;
    while (curr != nullptr) {
        if (view.is_visible(curr->xmin)) {
            return curr->old_val;
        }
        curr = curr->prev;
    }

    return std::nullopt; // Tuple was created after snapshot started
}

} // namespace db::tx
