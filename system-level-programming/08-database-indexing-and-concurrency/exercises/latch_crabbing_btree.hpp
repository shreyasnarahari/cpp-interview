#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <optional>
#include <algorithm>
#include <memory>
#include <iostream>

namespace db::index {

/**
 * @brief Concurrent B+ Tree with Read/Write Latch Crabbing Protocol.
 */
template <typename KeyType = uint64_t, typename ValueType = uint64_t, size_t MaxKeys = 32>
class ConcurrentBTree {
public:
    struct Node {
        bool is_leaf{true};
        mutable std::shared_mutex latch;
        std::vector<KeyType> keys;

        explicit Node(bool leaf) : is_leaf(leaf) {
            keys.reserve(MaxKeys + 1);
        }
        virtual ~Node() = default;

        [[nodiscard]] bool is_safe_for_insert() const noexcept {
            return keys.size() < MaxKeys;
        }
    };

    struct InternalNode : public Node {
        std::vector<std::shared_ptr<Node>> children;

        InternalNode() : Node(false) {
            children.reserve(MaxKeys + 2);
        }
    };

    struct LeafNode : public Node {
        std::vector<ValueType> values;
        std::shared_ptr<LeafNode> next{nullptr};

        LeafNode() : Node(true) {
            values.reserve(MaxKeys + 1);
        }
    };

    ConcurrentBTree() {
        root_ = std::make_shared<LeafNode>();
    }

    /**
     * @brief Point Lookup using Read Latch Crabbing (Root-to-Leaf).
     */
    [[nodiscard]] std::optional<ValueType> search(const KeyType& key) const {
        std::shared_lock<std::shared_mutex> root_guard(root_latch_);
        auto curr = root_;
        curr->latch.lock_shared();
        root_guard.unlock();

        while (!curr->is_leaf) {
            auto* internal = static_cast<InternalNode*>(curr.get());
            size_t idx = 0;
            while (idx < internal->keys.size() && key >= internal->keys[idx]) {
                idx++;
            }

            auto child = internal->children[idx];
            child->latch.lock_shared();
            curr->latch.unlock_shared();
            curr = child;
        }

        auto* leaf = static_cast<LeafNode*>(curr.get());
        std::optional<ValueType> result = std::nullopt;

        auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
        if (it != leaf->keys.end() && *it == key) {
            const size_t idx = static_cast<size_t>(std::distance(leaf->keys.begin(), it));
            result = leaf->values[idx];
        }

        leaf->latch.unlock_shared();
        return result;
    }

    /**
     * @brief Key Insertion with Write Latch Crabbing.
     */
    void insert(const KeyType& key, const ValueType& val) {
        std::unique_lock<std::shared_mutex> root_guard(root_latch_);

        if (root_->is_leaf) {
            auto* leaf = static_cast<LeafNode*>(root_.get());
            std::unique_lock<std::shared_mutex> leaf_lock(leaf->latch);

            insert_into_leaf(leaf, key, val);

            if (leaf->keys.size() > MaxKeys) {
                // Split root leaf
                auto new_root = std::make_shared<InternalNode>();
                auto new_leaf = std::make_shared<LeafNode>();

                const size_t mid = leaf->keys.size() / 2;
                const KeyType split_key = leaf->keys[mid];

                new_leaf->keys.assign(leaf->keys.begin() + mid, leaf->keys.end());
                new_leaf->values.assign(leaf->values.begin() + mid, leaf->values.end());

                leaf->keys.erase(leaf->keys.begin() + mid, leaf->keys.end());
                leaf->values.erase(leaf->values.begin() + mid, leaf->values.end());

                new_leaf->next = leaf->next;
                leaf->next = new_leaf;

                new_root->keys.push_back(split_key);
                new_root->children.push_back(root_);
                new_root->children.push_back(new_leaf);

                root_ = new_root;
            }
            return;
        }

        // Internal root
        auto curr = root_;
        curr->latch.lock();

        if (curr->is_safe_for_insert()) {
            root_guard.unlock();
        }

        std::vector<std::shared_ptr<Node>> locked_ancestors;
        locked_ancestors.push_back(curr);

        while (!curr->is_leaf) {
            auto* internal = static_cast<InternalNode*>(curr.get());
            size_t idx = 0;
            while (idx < internal->keys.size() && key >= internal->keys[idx]) {
                idx++;
            }

            auto child = internal->children[idx];
            child->latch.lock();

            if (child->is_safe_for_insert()) {
                for (auto& anc : locked_ancestors) {
                    anc->latch.unlock();
                }
                locked_ancestors.clear();
                if (root_guard.owns_lock()) {
                    root_guard.unlock();
                }
            }

            locked_ancestors.push_back(child);
            curr = child;
        }

        auto* leaf = static_cast<LeafNode*>(curr.get());
        insert_into_leaf(leaf, key, val);

        for (auto& anc : locked_ancestors) {
            anc->latch.unlock();
        }
    }

private:
    static void insert_into_leaf(LeafNode* leaf, const KeyType& key, const ValueType& val) {
        auto it = std::lower_bound(leaf->keys.begin(), leaf->keys.end(), key);
        const size_t idx = static_cast<size_t>(std::distance(leaf->keys.begin(), it));

        if (it != leaf->keys.end() && *it == key) {
            leaf->values[idx] = val; // Update existing
            return;
        }

        leaf->keys.insert(it, key);
        leaf->values.insert(leaf->values.begin() + idx, val);
    }

    std::shared_ptr<Node> root_;
    mutable std::shared_mutex root_latch_;
};

} // namespace db::index
