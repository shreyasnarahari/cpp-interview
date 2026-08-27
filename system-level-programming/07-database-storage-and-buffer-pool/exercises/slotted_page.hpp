#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <string>
#include <optional>
#include <cstring>
#include <vector>
#include <algorithm>

namespace db::storage {

/**
 * @brief 4KB Slotted Database Page.
 * 
 * Layout:
 * +----------------+----------------------+--------------------+--------------------+
 * | Header (24B)   | Slot Array (6B/slot) | Free Space Area    | Tuples Storage Area|
 * +----------------+----------------------+--------------------+--------------------+
 * 0                24                     free_space_ptr       4096
 */
class SlottedPage {
public:
    static constexpr size_t PAGE_SIZE = 4096;

    struct alignas(2) Slot {
        uint16_t offset{0};
        uint16_t size{0};
        uint8_t  is_deleted{0};
        uint8_t  padding{0};
    };

    struct Header {
        uint64_t page_id{0};
        uint64_t lsn{0};
        uint16_t slot_count{0};
        uint16_t free_space_ptr{PAGE_SIZE};
        uint32_t flags{0};
    };

    SlottedPage() noexcept {
        init(0);
    }

    explicit SlottedPage(uint64_t page_id) noexcept {
        init(page_id);
    }

    void init(uint64_t page_id) noexcept {
        std::memset(data_, 0, PAGE_SIZE);
        auto* hdr = header();
        hdr->page_id = page_id;
        hdr->lsn = 0;
        hdr->slot_count = 0;
        hdr->free_space_ptr = static_cast<uint16_t>(PAGE_SIZE);
        hdr->flags = 0;
    }

    [[nodiscard]] uint64_t page_id() const noexcept { return header()->page_id; }
    [[nodiscard]] uint64_t lsn() const noexcept { return header()->lsn; }
    void set_lsn(uint64_t lsn) noexcept { header()->lsn = lsn; }
    [[nodiscard]] uint16_t slot_count() const noexcept { return header()->slot_count; }

    [[nodiscard]] uint16_t free_space() const noexcept {
        const auto* hdr = header();
        const size_t header_and_slots = sizeof(Header) + (hdr->slot_count * sizeof(Slot));
        if (hdr->free_space_ptr < header_and_slots) return 0;
        return static_cast<uint16_t>(hdr->free_space_ptr - header_and_slots);
    }

    /**
     * @brief Inserts a variable-length tuple payload into the slotted page.
     */
    [[nodiscard]] bool insert_tuple(std::string_view payload, uint16_t& out_slot_id) noexcept {
        auto* hdr = header();
        const size_t needed_space = payload.size() + sizeof(Slot);

        if (free_space() < needed_space) {
            compact();
            if (free_space() < needed_space) {
                return false; // Not enough space even after compaction
            }
        }

        // Allocate tuple storage growing backward from free_space_ptr
        hdr->free_space_ptr -= static_cast<uint16_t>(payload.size());
        std::memcpy(data_ + hdr->free_space_ptr, payload.data(), payload.size());

        // Find reusable deleted slot or append new slot
        uint16_t target_slot = hdr->slot_count;
        for (uint16_t i = 0; i < hdr->slot_count; ++i) {
            if (slot(i)->is_deleted) {
                target_slot = i;
                break;
            }
        }

        Slot* s = slot(target_slot);
        s->offset = hdr->free_space_ptr;
        s->size = static_cast<uint16_t>(payload.size());
        s->is_deleted = 0;

        if (target_slot == hdr->slot_count) {
            hdr->slot_count++;
        }

        out_slot_id = target_slot;
        return true;
    }

    /**
     * @brief Reads a tuple payload by slot ID.
     */
    [[nodiscard]] bool get_tuple(uint16_t slot_id, std::string& out_payload) const {
        if (slot_id >= header()->slot_count) return false;
        const Slot* s = slot(slot_id);
        if (s->is_deleted) return false;

        out_payload.assign(reinterpret_cast<const char*>(data_ + s->offset), s->size);
        return true;
    }

    /**
     * @brief Updates an existing tuple payload.
     */
    [[nodiscard]] bool update_tuple(uint16_t slot_id, std::string_view new_payload) noexcept {
        if (slot_id >= header()->slot_count) return false;
        Slot* s = slot(slot_id);
        if (s->is_deleted) return false;

        if (new_payload.size() <= s->size) {
            // Fits in-place
            std::memcpy(data_ + s->offset, new_payload.data(), new_payload.size());
            s->size = static_cast<uint16_t>(new_payload.size());
            return true;
        }

        // Reallocate tuple in free space
        auto* hdr = header();
        if (free_space() < new_payload.size()) {
            compact();
            if (free_space() < new_payload.size()) {
                return false; // Out of space
            }
        }

        hdr->free_space_ptr -= static_cast<uint16_t>(new_payload.size());
        std::memcpy(data_ + hdr->free_space_ptr, new_payload.data(), new_payload.size());

        s->offset = hdr->free_space_ptr;
        s->size = static_cast<uint16_t>(new_payload.size());
        return true;
    }

    /**
     * @brief Deletes a tuple by slot ID.
     */
    [[nodiscard]] bool delete_tuple(uint16_t slot_id) noexcept {
        if (slot_id >= header()->slot_count) return false;
        Slot* s = slot(slot_id);
        if (s->is_deleted) return false;

        s->is_deleted = 1;
        s->size = 0;
        return true;
    }

    /**
     * @brief Defragments and compacts surviving tuples into a contiguous block at the end of the page.
     */
    void compact() noexcept {
        auto* hdr = header();
        alignas(64) uint8_t temp_buf[PAGE_SIZE];
        uint16_t new_free_ptr = static_cast<uint16_t>(PAGE_SIZE);

        for (uint16_t i = 0; i < hdr->slot_count; ++i) {
            Slot* s = slot(i);
            if (!s->is_deleted && s->size > 0) {
                new_free_ptr -= s->size;
                std::memcpy(temp_buf + new_free_ptr, data_ + s->offset, s->size);
                s->offset = new_free_ptr;
            }
        }

        // Copy compacted tuples back to data_
        const size_t compacted_bytes = PAGE_SIZE - new_free_ptr;
        if (compacted_bytes > 0) {
            std::memcpy(data_ + new_free_ptr, temp_buf + new_free_ptr, compacted_bytes);
        }

        hdr->free_space_ptr = new_free_ptr;
    }

    [[nodiscard]] const uint8_t* raw_data() const noexcept { return data_; }
    [[nodiscard]] uint8_t* raw_data() noexcept { return data_; }

private:
    [[nodiscard]] Header* header() noexcept {
        return reinterpret_cast<Header*>(data_);
    }

    [[nodiscard]] const Header* header() const noexcept {
        return reinterpret_cast<const Header*>(data_);
    }

    [[nodiscard]] Slot* slot(uint16_t idx) noexcept {
        return reinterpret_cast<Slot*>(data_ + sizeof(Header) + (idx * sizeof(Slot)));
    }

    [[nodiscard]] const Slot* slot(uint16_t idx) const noexcept {
        return reinterpret_cast<const Slot*>(data_ + sizeof(Header) + (idx * sizeof(Slot)));
    }

    alignas(64) uint8_t data_[PAGE_SIZE];
};

} // namespace db::storage
