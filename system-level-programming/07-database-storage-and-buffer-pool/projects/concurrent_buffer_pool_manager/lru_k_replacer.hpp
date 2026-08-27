#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <limits>
#include <algorithm>

namespace db::storage {

using frame_id_t = uint32_t;

/**
 * @brief LRU-K (k=2) Buffer Frame Replacer.
 * 
 * Tracks the backward k-distance for each frame.
 * Prioritizes frames with < K accesses (infinite distance) in FIFO order of their earliest access,
 * and then frames with the largest backward k-distance.
 */
class LRUKReplacer {
public:
    explicit LRUKReplacer(size_t num_frames, size_t k = 2)
        : num_frames_(num_frames), k_(k) {}

    void record_access(frame_id_t frame_id) {
        std::lock_guard<std::mutex> lock(latch_);
        current_timestamp_++;

        auto& node = entries_[frame_id];
        node.history.push_back(current_timestamp_);
        if (node.history.size() > k_) {
            node.history.pop_front();
        }
    }

    void set_evictable(frame_id_t frame_id, bool set_evictable) {
        std::lock_guard<std::mutex> lock(latch_);
        auto it = entries_.find(frame_id);
        if (it != entries_.end()) {
            if (it->second.is_evictable != set_evictable) {
                it->second.is_evictable = set_evictable;
                if (set_evictable) {
                    curr_size_++;
                } else {
                    curr_size_--;
                }
            }
        }
    }

    [[nodiscard]] bool victim(frame_id_t& out_frame_id) {
        std::lock_guard<std::mutex> lock(latch_);
        if (curr_size_ == 0) return false;

        frame_id_t victim_id = 0;
        uint64_t max_k_distance = 0;
        uint64_t earliest_inf_timestamp = std::numeric_limits<uint64_t>::max();
        bool found_inf = false;
        bool found_any = false;

        for (const auto& [fid, entry] : entries_) {
            if (!entry.is_evictable) continue;

            if (entry.history.size() < k_) {
                // Infinite backward distance
                const uint64_t earliest = entry.history.empty() ? 0 : entry.history.front();
                if (!found_inf || earliest < earliest_inf_timestamp) {
                    found_inf = true;
                    earliest_inf_timestamp = earliest;
                    victim_id = fid;
                    found_any = true;
                }
            } else if (!found_inf) {
                // Finite backward distance
                const uint64_t k_distance = current_timestamp_ - entry.history.front();
                if (!found_any || k_distance > max_k_distance) {
                    max_k_distance = k_distance;
                    victim_id = fid;
                    found_any = true;
                }
            }
        }

        if (found_any) {
            out_frame_id = victim_id;
            entries_.erase(victim_id);
            curr_size_--;
            return true;
        }

        return false;
    }

    void remove(frame_id_t frame_id) {
        std::lock_guard<std::mutex> lock(latch_);
        auto it = entries_.find(frame_id);
        if (it != entries_.end()) {
            if (it->second.is_evictable) {
                curr_size_--;
            }
            entries_.erase(it);
        }
    }

    [[nodiscard]] size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(latch_);
        return curr_size_;
    }

private:
    struct FrameNode {
        std::deque<uint64_t> history;
        bool is_evictable{false};
    };

    const size_t num_frames_;
    const size_t k_;
    size_t curr_size_{0};
    uint64_t current_timestamp_{0};
    std::unordered_map<frame_id_t, FrameNode> entries_;
    mutable std::mutex latch_;
};

} // namespace db::storage
