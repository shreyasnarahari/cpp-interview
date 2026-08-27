#pragma once

#include "disk_manager.hpp"
#include "lru_k_replacer.hpp"
#include "../../exercises/slotted_page.hpp"
#include <vector>
#include <list>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <memory>
#include <iostream>

namespace db::storage {

/**
 * @brief Thread-Safe Concurrent Buffer Pool Manager.
 */
class BufferPoolManager {
public:
    static constexpr uint64_t INVALID_PAGE_ID = std::numeric_limits<uint64_t>::max();

    BufferPoolManager(size_t pool_size, std::string db_file, size_t k = 2)
        : pool_size_(pool_size),
          disk_manager_(std::move(db_file)),
          replacer_(pool_size, k),
          pages_(pool_size),
          frames_(pool_size) {
        for (frame_id_t i = 0; i < static_cast<frame_id_t>(pool_size_); ++i) {
            free_list_.push_back(i);
            frames_[i].page_id = INVALID_PAGE_ID;
        }
    }

    ~BufferPoolManager() {
        flush_all_pages();
    }

    BufferPoolManager(const BufferPoolManager&) = delete;
    BufferPoolManager& operator=(const BufferPoolManager&) = delete;

    /**
     * @brief Creates and pins a new page in the buffer pool.
     */
    SlottedPage* new_page(uint64_t& out_page_id) {
        std::lock_guard<std::shared_mutex> lock(bpm_latch_);

        frame_id_t fid = 0;
        if (!find_available_frame(fid)) {
            return nullptr; // No free or evictable frames
        }

        const uint64_t page_id = disk_manager_.allocate_page();
        out_page_id = page_id;

        // If frame had old page, flush if dirty
        if (frames_[fid].page_id != INVALID_PAGE_ID) {
            if (frames_[fid].is_dirty) {
                disk_manager_.write_page(frames_[fid].page_id, pages_[fid].raw_data());
                frames_[fid].is_dirty = false;
            }
            page_table_.erase(frames_[fid].page_id);
        }

        // Setup new page
        pages_[fid].init(page_id);
        page_table_[page_id] = fid;
        frames_[fid].page_id = page_id;
        frames_[fid].pin_count = 1;
        frames_[fid].is_dirty = false;

        replacer_.record_access(fid);
        replacer_.set_evictable(fid, false);

        return &pages_[fid];
    }

    /**
     * @brief Fetches and pins an existing page from disk / buffer pool.
     */
    SlottedPage* fetch_page(uint64_t page_id) {
        std::lock_guard<std::shared_mutex> lock(bpm_latch_);

        auto it = page_table_.find(page_id);
        if (it != page_table_.end()) {
            const frame_id_t fid = it->second;
            frames_[fid].pin_count++;
            replacer_.record_access(fid);
            replacer_.set_evictable(fid, false);
            return &pages_[fid];
        }

        // Cache miss: Find frame to load page from disk
        frame_id_t fid = 0;
        if (!find_available_frame(fid)) {
            return nullptr; // Out of memory frames
        }

        // Evict previous frame if dirty
        if (frames_[fid].page_id != INVALID_PAGE_ID) {
            if (frames_[fid].is_dirty) {
                disk_manager_.write_page(frames_[fid].page_id, pages_[fid].raw_data());
                frames_[fid].is_dirty = false;
            }
            page_table_.erase(frames_[fid].page_id);
        }

        // Read page from disk
        disk_manager_.read_page(page_id, pages_[fid].raw_data());

        page_table_[page_id] = fid;
        frames_[fid].page_id = page_id;
        frames_[fid].pin_count = 1;
        frames_[fid].is_dirty = false;

        replacer_.record_access(fid);
        replacer_.set_evictable(fid, false);

        return &pages_[fid];
    }

    /**
     * @brief Unpins a page, updating its dirty status.
     */
    bool unpin_page(uint64_t page_id, bool is_dirty) {
        std::lock_guard<std::shared_mutex> lock(bpm_latch_);

        auto it = page_table_.find(page_id);
        if (it == page_table_.end()) return false;

        const frame_id_t fid = it->second;
        if (frames_[fid].pin_count <= 0) return false;

        if (is_dirty) {
            frames_[fid].is_dirty = true;
        }

        frames_[fid].pin_count--;
        if (frames_[fid].pin_count == 0) {
            replacer_.set_evictable(fid, true);
        }

        return true;
    }

    /**
     * @brief Flushes a page to disk.
     */
    bool flush_page(uint64_t page_id) {
        std::lock_guard<std::shared_mutex> lock(bpm_latch_);

        auto it = page_table_.find(page_id);
        if (it == page_table_.end()) return false;

        const frame_id_t fid = it->second;
        disk_manager_.write_page(page_id, pages_[fid].raw_data());
        frames_[fid].is_dirty = false;
        return true;
    }

    void flush_all_pages() {
        std::lock_guard<std::shared_mutex> lock(bpm_latch_);
        for (const auto& [page_id, fid] : page_table_) {
            if (frames_[fid].is_dirty) {
                disk_manager_.write_page(page_id, pages_[fid].raw_data());
                frames_[fid].is_dirty = false;
            }
        }
    }

    [[nodiscard]] size_t pool_size() const noexcept { return pool_size_; }

private:
    struct FrameMeta {
        uint64_t page_id{0};
        int32_t pin_count{0};
        bool is_dirty{false};
    };

    bool find_available_frame(frame_id_t& out_fid) {
        if (!free_list_.empty()) {
            out_fid = free_list_.front();
            free_list_.pop_front();
            return true;
        }
        return replacer_.victim(out_fid);
    }

    const size_t pool_size_;
    DiskManager disk_manager_;
    LRUKReplacer replacer_;

    std::vector<SlottedPage> pages_;
    std::vector<FrameMeta> frames_;
    std::list<frame_id_t> free_list_;
    std::unordered_map<uint64_t, frame_id_t> page_table_;
    std::shared_mutex bpm_latch_;
};

} // namespace db::storage
