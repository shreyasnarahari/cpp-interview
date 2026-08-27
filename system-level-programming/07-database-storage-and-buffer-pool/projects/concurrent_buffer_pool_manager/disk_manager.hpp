#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <fstream>
#include <mutex>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>

namespace db::storage {

class DiskManager {
public:
    static constexpr size_t PAGE_SIZE = 4096;

    explicit DiskManager(std::string db_filename)
        : db_filename_(std::move(db_filename)) {
        fd_ = ::open(db_filename_.c_str(), O_CREAT | O_RDWR, 0644);
        if (fd_ < 0) {
            throw std::runtime_error("Failed to open database file: " + db_filename_);
        }
    }

    ~DiskManager() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    void write_page(uint64_t page_id, const uint8_t* page_data) {
        std::lock_guard<std::mutex> lock(db_io_latch_);
        const off_t offset = static_cast<off_t>(page_id * PAGE_SIZE);
        const ssize_t written = ::pwrite(fd_, page_data, PAGE_SIZE, offset);
        if (written != static_cast<ssize_t>(PAGE_SIZE)) {
            throw std::runtime_error("Disk write failed for page " + std::to_string(page_id));
        }
    }

    void read_page(uint64_t page_id, uint8_t* page_data) {
        std::lock_guard<std::mutex> lock(db_io_latch_);
        const off_t offset = static_cast<off_t>(page_id * PAGE_SIZE);
        const ssize_t bytes_read = ::pread(fd_, page_data, PAGE_SIZE, offset);
        if (bytes_read <= 0) {
            // Unwritten page initialized with zeros
            std::memset(page_data, 0, PAGE_SIZE);
        }
    }

    uint64_t allocate_page() {
        std::lock_guard<std::mutex> lock(db_io_latch_);
        return next_page_id_++;
    }

private:
    std::string db_filename_;
    int fd_{-1};
    uint64_t next_page_id_{0};
    std::mutex db_io_latch_;
};

} // namespace db::storage
