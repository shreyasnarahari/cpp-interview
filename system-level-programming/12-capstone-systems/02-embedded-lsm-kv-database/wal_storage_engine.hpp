#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <optional>
#include <fstream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdexcept>
#include <iostream>

namespace sys::storage {

enum class WalOp : uint32_t {
    PUT = 1,
    DEL = 2
};

struct alignas(4096) WalBlock {
    static constexpr uint32_t MAGIC = 0x57414C31; // "WAL1"

    uint32_t magic{MAGIC};
    uint32_t checksum{0};
    uint64_t seq_num{0};
    uint64_t timestamp_ns{0};
    WalOp op{WalOp::PUT};
    uint32_t padding{0};
    uint64_t key{0};
    uint64_t value{0};
    char reserved[4096 - 48]{0};

    [[nodiscard]] uint32_t compute_checksum() const noexcept {
        uint32_t crc = 0x811C9DC5; // FNV-1a basis
        const auto* bytes = reinterpret_cast<const uint8_t*>(&seq_num);
        constexpr size_t payload_len = sizeof(WalBlock) - 8; // Exclude magic & checksum
        for (size_t i = 0; i < payload_len; ++i) {
            crc = (crc ^ bytes[i]) * 0x01000193;
        }
        return crc;
    }
};

class WalStorageEngine {
public:
    explicit WalStorageEngine(std::string wal_path)
        : wal_path_(std::move(wal_path)) {}

    ~WalStorageEngine() {
        close();
    }

    WalStorageEngine(const WalStorageEngine&) = delete;
    WalStorageEngine& operator=(const WalStorageEngine&) = delete;

    bool open() {
        fd_ = ::open(wal_path_.c_str(), O_CREAT | O_RDWR, 0644);
        if (fd_ < 0) {
            return false;
        }
        return recover();
    }

    void close() {
        if (fd_ >= 0) {
            ::fdatasync(fd_);
            ::close(fd_);
            fd_ = -1;
        }
        index_.clear();
    }

    bool put(uint64_t key, uint64_t value) {
        WalBlock block{};
        block.magic = WalBlock::MAGIC;
        block.seq_num = ++current_seq_;
        block.timestamp_ns = 0;
        block.op = WalOp::PUT;
        block.key = key;
        block.value = value;
        block.checksum = block.compute_checksum();

        const ssize_t written = ::write(fd_, &block, sizeof(WalBlock));
        if (written != sizeof(WalBlock)) {
            return false;
        }

        index_[key] = value;
        return true;
    }

    [[nodiscard]] std::optional<uint64_t> get(uint64_t key) const {
        auto it = index_.find(key);
        if (it != index_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool del(uint64_t key) {
        WalBlock block{};
        block.magic = WalBlock::MAGIC;
        block.seq_num = ++current_seq_;
        block.timestamp_ns = 0;
        block.op = WalOp::DEL;
        block.key = key;
        block.value = 0;
        block.checksum = block.compute_checksum();

        const ssize_t written = ::write(fd_, &block, sizeof(WalBlock));
        if (written != sizeof(WalBlock)) {
            return false;
        }

        index_.erase(key);
        return true;
    }

    [[nodiscard]] size_t size() const noexcept {
        return index_.size();
    }

    bool recover() {
        index_.clear();
        current_seq_ = 0;

        if (::lseek(fd_, 0, SEEK_SET) < 0) {
            return false;
        }

        WalBlock block;
        while (true) {
            const ssize_t bytes = ::read(fd_, &block, sizeof(WalBlock));
            if (bytes == 0) break; // EOF
            if (bytes != sizeof(WalBlock)) {
                // Partial block write; truncate and recover
                break;
            }

            if (block.magic != WalBlock::MAGIC) {
                break; // Corrupted magic
            }

            if (block.checksum != block.compute_checksum()) {
                break; // Corrupted checksum
            }

            current_seq_ = std::max(current_seq_, block.seq_num);

            if (block.op == WalOp::PUT) {
                index_[block.key] = block.value;
            } else if (block.op == WalOp::DEL) {
                index_.erase(block.key);
            }
        }

        // Seek back to end for next appends
        ::lseek(fd_, 0, SEEK_END);
        return true;
    }

private:
    std::string wal_path_;
    int fd_{-1};
    uint64_t current_seq_{0};
    std::unordered_map<uint64_t, uint64_t> index_;
};

} // namespace sys::storage
