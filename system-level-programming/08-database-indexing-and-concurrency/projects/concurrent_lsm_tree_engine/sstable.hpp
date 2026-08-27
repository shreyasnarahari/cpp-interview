#pragma once

#include "bloom_filter.hpp"
#include "skiplist_memtable.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>

namespace db::lsm {

class SSTable {
public:
    struct BlockIndexEntry {
        uint64_t last_key{0};
        uint64_t offset{0};
        uint64_t count{0};
    };

    struct Footer {
        static constexpr uint32_t MAGIC = 0x53535431; // "SST1"
        uint32_t magic{MAGIC};
        uint64_t index_offset{0};
        uint64_t index_count{0};
        uint64_t bloom_offset{0};
        uint64_t bloom_num_bits{0};
        uint64_t bloom_num_hashes{0};
    };

    static bool build(const std::string& filename, const SkipListMemTable& memtable) {
        const auto pairs = memtable.to_vector();
        if (pairs.empty()) return false;

        int fd = ::open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) return false;

        BloomFilter bloom(pairs.size(), 0.01);
        for (const auto& [k, v] : pairs) {
            bloom.add(k);
        }

        std::vector<BlockIndexEntry> index;
        constexpr size_t RECORDS_PER_BLOCK = 128;

        uint64_t current_offset = 0;
        for (size_t i = 0; i < pairs.size(); i += RECORDS_PER_BLOCK) {
            const size_t chunk_size = std::min(RECORDS_PER_BLOCK, pairs.size() - i);
            const size_t bytes_to_write = chunk_size * sizeof(std::pair<uint64_t, uint64_t>);

            ssize_t w = ::write(fd, &pairs[i], bytes_to_write); (void)w;

            BlockIndexEntry entry{};
            entry.last_key = pairs[i + chunk_size - 1].first;
            entry.offset = current_offset;
            entry.count = chunk_size;
            index.push_back(entry);

            current_offset += bytes_to_write;
        }

        // Write Index Block
        const uint64_t index_offset = current_offset;
        const size_t index_bytes = index.size() * sizeof(BlockIndexEntry);
        ssize_t w1 = ::write(fd, index.data(), index_bytes); (void)w1;
        current_offset += index_bytes;

        // Write Bloom Filter
        const uint64_t bloom_offset = current_offset;
        const auto& bloom_bits = bloom.raw_bits();
        const size_t bloom_bytes = bloom_bits.size() * sizeof(uint64_t);
        ssize_t w2 = ::write(fd, bloom_bits.data(), bloom_bytes); (void)w2;
        current_offset += bloom_bytes;

        // Write Footer
        Footer footer{};
        footer.index_offset = index_offset;
        footer.index_count = index.size();
        footer.bloom_offset = bloom_offset;
        footer.bloom_num_bits = bloom.num_bits();
        footer.bloom_num_hashes = bloom.num_hashes();

        ssize_t w3 = ::write(fd, &footer, sizeof(Footer)); (void)w3;
        ::close(fd);
        return true;
    }

    static std::unique_ptr<SSTable> open(const std::string& filename) {
        int fd = ::open(filename.c_str(), O_RDONLY, 0644);
        if (fd < 0) return nullptr;

        const off_t file_sz = ::lseek(fd, 0, SEEK_END);
        if (file_sz < static_cast<off_t>(sizeof(Footer))) {
            ::close(fd);
            return nullptr;
        }

        Footer footer{};
        ssize_t r0 = ::pread(fd, &footer, sizeof(Footer), file_sz - sizeof(Footer)); (void)r0;
        if (footer.magic != Footer::MAGIC) {
            ::close(fd);
            return nullptr;
        }

        auto sstable = std::unique_ptr<SSTable>(new SSTable(filename, fd));

        // Load Index Block
        sstable->index_.resize(footer.index_count);
        ssize_t r1 = ::pread(fd, sstable->index_.data(), footer.index_count * sizeof(BlockIndexEntry), static_cast<off_t>(footer.index_offset)); (void)r1;

        // Load Bloom Filter
        const size_t bloom_words = (footer.bloom_num_bits + 63) / 64;
        std::vector<uint64_t> bloom_bits(bloom_words);
        ssize_t r2 = ::pread(fd, bloom_bits.data(), bloom_words * sizeof(uint64_t), static_cast<off_t>(footer.bloom_offset)); (void)r2;
        sstable->bloom_.set_raw_bits(std::move(bloom_bits), footer.bloom_num_bits, footer.bloom_num_hashes);

        return sstable;
    }

    ~SSTable() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    [[nodiscard]] bool get(uint64_t key, uint64_t& out_val) const {
        // Step 1: Bloom filter fast negative check (0 disk I/O)
        if (!bloom_.contains(key)) {
            return false;
        }

        // Step 2: Binary search in index block
        size_t low = 0;
        size_t high = index_.size();
        size_t block_idx = index_.size();

        while (low < high) {
            size_t mid = low + (high - low) / 2;
            if (index_[mid].last_key >= key) {
                block_idx = mid;
                high = mid;
            } else {
                low = mid + 1;
            }
        }

        if (block_idx >= index_.size()) {
            return false;
        }

        // Step 3: Read data block from disk
        const auto& block_entry = index_[block_idx];
        std::vector<std::pair<uint64_t, uint64_t>> records(block_entry.count);
        ssize_t r3 = ::pread(fd_, records.data(), block_entry.count * sizeof(std::pair<uint64_t, uint64_t>), static_cast<off_t>(block_entry.offset)); (void)r3;

        // Step 4: Binary search inside data block
        auto it = std::lower_bound(records.begin(), records.end(), key, 
            [](const std::pair<uint64_t, uint64_t>& p, uint64_t k) {
                return p.first < k;
            });

        if (it != records.end() && it->first == key) {
            out_val = it->second;
            return true;
        }

        return false;
    }

    SSTable(std::string filename, int fd)
        : filename_(std::move(filename)), fd_(fd) {}

    std::string filename_;
    int fd_{-1};
    BloomFilter bloom_;
    std::vector<BlockIndexEntry> index_;
};

} // namespace db::lsm
