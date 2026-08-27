#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <mutex>
#include <random>
#include <memory>

namespace db::lsm {

class SkipListMemTable {
public:
    static constexpr size_t MAX_LEVEL = 16;

    struct Node {
        uint64_t key{0};
        uint64_t value{0};
        size_t level{1};
        std::vector<Node*> forward;

        Node(uint64_t k, uint64_t v, size_t lvl)
            : key(k), value(v), level(lvl), forward(lvl, nullptr) {}
    };

    SkipListMemTable()
        : head_(new Node(0, 0, MAX_LEVEL)),
          rng_(1337),
          dist_(0.0, 1.0) {}

    ~SkipListMemTable() {
        Node* curr = head_;
        while (curr) {
            Node* next = curr->forward[0];
            delete curr;
            curr = next;
        }
    }

    SkipListMemTable(const SkipListMemTable&) = delete;
    SkipListMemTable& operator=(const SkipListMemTable&) = delete;

    void put(uint64_t key, uint64_t value) {
        std::unique_lock<std::shared_mutex> lock(latch_);

        std::vector<Node*> update(MAX_LEVEL, nullptr);
        Node* curr = head_;

        for (int i = static_cast<int>(current_level_) - 1; i >= 0; --i) {
            while (curr->forward[i] && curr->forward[i]->key < key) {
                curr = curr->forward[i];
            }
            update[i] = curr;
        }

        curr = curr->forward[0];

        if (curr && curr->key == key) {
            curr->value = value; // Update existing
            return;
        }

        const size_t new_level = random_level();
        if (new_level > current_level_) {
            for (size_t i = current_level_; i < new_level; ++i) {
                update[i] = head_;
            }
            current_level_ = new_level;
        }

        auto* new_node = new Node(key, value, new_level);
        for (size_t i = 0; i < new_level; ++i) {
            new_node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = new_node;
        }

        size_++;
        byte_size_ += sizeof(Node) + (new_level * sizeof(Node*));
    }

    [[nodiscard]] bool get(uint64_t key, uint64_t& out_val) const {
        std::shared_lock<std::shared_mutex> lock(latch_);

        Node* curr = head_;
        for (int i = static_cast<int>(current_level_) - 1; i >= 0; --i) {
            while (curr->forward[i] && curr->forward[i]->key < key) {
                curr = curr->forward[i];
            }
        }

        curr = curr->forward[0];
        if (curr && curr->key == key) {
            out_val = curr->value;
            return true;
        }
        return false;
    }

    [[nodiscard]] std::vector<std::pair<uint64_t, uint64_t>> to_vector() const {
        std::shared_lock<std::shared_mutex> lock(latch_);
        std::vector<std::pair<uint64_t, uint64_t>> result;
        result.reserve(size_);

        Node* curr = head_->forward[0];
        while (curr) {
            result.emplace_back(curr->key, curr->value);
            curr = curr->forward[0];
        }
        return result;
    }

    [[nodiscard]] size_t size() const noexcept {
        std::shared_lock<std::shared_mutex> lock(latch_);
        return size_;
    }

    [[nodiscard]] size_t byte_size() const noexcept {
        std::shared_lock<std::shared_mutex> lock(latch_);
        return byte_size_;
    }

private:
    size_t random_level() {
        size_t lvl = 1;
        while (dist_(rng_) < 0.5 && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    Node* head_;
    size_t current_level_{1};
    size_t size_{0};
    size_t byte_size_{0};
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;
    mutable std::shared_mutex latch_;
};

} // namespace db::lsm
