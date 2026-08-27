#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/io_uring.h>
#include <cstring>
#include <iostream>
#include <atomic>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

namespace sys::net {

/**
 * @brief Raw Linux io_uring System Call Wrappers.
 */
inline int sys_io_uring_setup(uint32_t entries, struct io_uring_params* p) noexcept {
    return static_cast<int>(::syscall(__NR_io_uring_setup, entries, p));
}

inline int sys_io_uring_enter(int fd, uint32_t to_submit, uint32_t min_complete, uint32_t flags, sigset_t* sig) noexcept {
    return static_cast<int>(::syscall(__NR_io_uring_enter, fd, to_submit, min_complete, flags, sig));
}

/**
 * @brief Event State for Async io_uring operations.
 */
enum class EventType : uint8_t {
    ACCEPT,
    READ,
    WRITE
};

struct ConnectionContext {
    int fd{-1};
    EventType type{EventType::ACCEPT};
    uint32_t bytes_transferred{0};
    char buffer[2048]{0};
    struct sockaddr_in client_addr{};
    socklen_t client_len{sizeof(client_addr)};
};

/**
 * @brief High-Throughput Asynchronous TCP Echo Reactor built on Linux io_uring.
 */
class IoUringReactor {
public:
    static constexpr size_t QUEUE_DEPTH = 512;

    explicit IoUringReactor(uint16_t port) : port_(port) {}

    ~IoUringReactor() {
        stop();
    }

    bool init() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (listen_fd_ < 0) {
            std::cerr << "Failed to create socket: " << std::strerror(errno) << "\n";
            return false;
        }

        int opt = 1;
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (::bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::cerr << "Failed to bind to port " << port_ << ": " << std::strerror(errno) << "\n";
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        if (::listen(listen_fd_, SOMAXCONN) < 0) {
            std::cerr << "Failed to listen: " << std::strerror(errno) << "\n";
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        std::memset(&params_, 0, sizeof(params_));
        ring_fd_ = sys_io_uring_setup(QUEUE_DEPTH, &params_);
        if (ring_fd_ < 0) {
            std::cerr << "io_uring_setup failed: " << std::strerror(errno) << "\n";
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        // Map Submission Queue (SQ)
        const size_t sq_ring_sz = params_.sq_off.array + params_.sq_entries * sizeof(uint32_t);
        const size_t sqes_sz = params_.sq_entries * sizeof(struct io_uring_sqe);

        sq_ring_ptr_ = static_cast<uint8_t*>(::mmap(nullptr, sq_ring_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd_, IORING_OFF_SQ_RING));
        sqes_ = static_cast<struct io_uring_sqe*>(::mmap(nullptr, sqes_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd_, IORING_OFF_SQES));

        // Map Completion Queue (CQ)
        const size_t cq_ring_sz = params_.cq_off.cqes + params_.cq_entries * sizeof(struct io_uring_cqe);
        cq_ring_ptr_ = static_cast<uint8_t*>(::mmap(nullptr, cq_ring_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd_, IORING_OFF_CQ_RING));

        if (sq_ring_ptr_ == MAP_FAILED || sqes_ == MAP_FAILED || cq_ring_ptr_ == MAP_FAILED) {
            std::cerr << "Failed to mmap io_uring rings\n";
            return false;
        }

        // Initialize queue pointers
        sq_head_ = reinterpret_cast<uint32_t*>(sq_ring_ptr_ + params_.sq_off.head);
        sq_tail_ = reinterpret_cast<uint32_t*>(sq_ring_ptr_ + params_.sq_off.tail);
        sq_mask_ = *reinterpret_cast<uint32_t*>(sq_ring_ptr_ + params_.sq_off.ring_mask);
        sq_array_ = reinterpret_cast<uint32_t*>(sq_ring_ptr_ + params_.sq_off.array);

        cq_head_ = reinterpret_cast<uint32_t*>(cq_ring_ptr_ + params_.cq_off.head);
        cq_tail_ = reinterpret_cast<uint32_t*>(cq_ring_ptr_ + params_.cq_off.tail);
        cq_mask_ = *reinterpret_cast<uint32_t*>(cq_ring_ptr_ + params_.cq_off.ring_mask);
        cqes_ = reinterpret_cast<struct io_uring_cqe*>(cq_ring_ptr_ + params_.cq_off.cqes);

        // Submit initial async accept
        submit_accept();
        return true;
    }

    void run() {
        running_.store(true, std::memory_order_release);

        while (running_.load(std::memory_order_relaxed)) {
            if (pending_sqes_ > 0) {
                sys_io_uring_enter(ring_fd_, pending_sqes_, 0, 0, nullptr);
                pending_sqes_ = 0;
            }

            uint32_t head = *cq_head_;
            const uint32_t tail = *cq_tail_;

            if (head == tail) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }

            while (head != tail) {
                struct io_uring_cqe* cqe = &cqes_[head & cq_mask_];
                auto* ctx = reinterpret_cast<ConnectionContext*>(cqe->user_data);

                if (ctx) {
                    handle_completion(ctx, cqe->res);
                }

                ++head;
            }

            *cq_head_ = head;
        }
    }

    void stop() {
        running_.store(false, std::memory_order_release);
        if (ring_fd_ >= 0) {
            ::close(ring_fd_);
            ring_fd_ = -1;
        }
        if (listen_fd_ >= 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
    }

    [[nodiscard]] uint16_t get_port() const noexcept { return port_; }
    [[nodiscard]] uint64_t get_total_bytes_echoed() const noexcept { return total_bytes_echoed_.load(std::memory_order_relaxed); }

private:
    struct io_uring_sqe* get_sqe() {
        const uint32_t tail = *sq_tail_;
        const uint32_t head = *sq_head_;

        if (tail - head >= params_.sq_entries) {
            return nullptr; // SQ full
        }

        const uint32_t index = tail & sq_mask_;
        struct io_uring_sqe* sqe = &sqes_[index];
        std::memset(sqe, 0, sizeof(*sqe));
        sq_array_[index] = index;
        *sq_tail_ = tail + 1;
        ++pending_sqes_;
        return sqe;
    }

    void submit_accept() {
        struct io_uring_sqe* sqe = get_sqe();
        if (!sqe) return;

        auto* ctx = new ConnectionContext();
        ctx->fd = listen_fd_;
        ctx->type = EventType::ACCEPT;

        sqe->opcode = IORING_OP_ACCEPT;
        sqe->fd = listen_fd_;
        sqe->addr = reinterpret_cast<uint64_t>(&ctx->client_addr);
        sqe->addr2 = reinterpret_cast<uint64_t>(&ctx->client_len);
        sqe->flags = 0;
        sqe->user_data = reinterpret_cast<uint64_t>(ctx);
    }

    void submit_read(ConnectionContext* ctx) {
        struct io_uring_sqe* sqe = get_sqe();
        if (!sqe) return;

        ctx->type = EventType::READ;
        sqe->opcode = IORING_OP_READ;
        sqe->fd = ctx->fd;
        sqe->addr = reinterpret_cast<uint64_t>(ctx->buffer);
        sqe->len = sizeof(ctx->buffer);
        sqe->user_data = reinterpret_cast<uint64_t>(ctx);
    }

    void submit_write(ConnectionContext* ctx, uint32_t len) {
        struct io_uring_sqe* sqe = get_sqe();
        if (!sqe) return;

        ctx->type = EventType::WRITE;
        ctx->bytes_transferred = len;
        sqe->opcode = IORING_OP_WRITE;
        sqe->fd = ctx->fd;
        sqe->addr = reinterpret_cast<uint64_t>(ctx->buffer);
        sqe->len = len;
        sqe->user_data = reinterpret_cast<uint64_t>(ctx);
    }

    void handle_completion(ConnectionContext* ctx, int res) {
        switch (ctx->type) {
            case EventType::ACCEPT: {
                if (res >= 0) {
                    auto* client_ctx = new ConnectionContext();
                    client_ctx->fd = res;
                    submit_read(client_ctx);
                }
                // Submit next accept
                submit_accept();
                delete ctx;
                break;
            }
            case EventType::READ: {
                if (res > 0) {
                    total_bytes_echoed_.fetch_add(static_cast<uint64_t>(res), std::memory_order_relaxed);
                    submit_write(ctx, static_cast<uint32_t>(res));
                } else {
                    // Client closed connection or error
                    ::close(ctx->fd);
                    delete ctx;
                }
                break;
            }
            case EventType::WRITE: {
                if (res > 0) {
                    // Loop back to read next message
                    submit_read(ctx);
                } else {
                    ::close(ctx->fd);
                    delete ctx;
                }
                break;
            }
        }
    }

    uint16_t port_;
    int listen_fd_{-1};
    int ring_fd_{-1};
    uint32_t pending_sqes_{0};

    struct io_uring_params params_{};
    uint8_t* sq_ring_ptr_{nullptr};
    uint8_t* cq_ring_ptr_{nullptr};
    struct io_uring_sqe* sqes_{nullptr};
    struct io_uring_cqe* cqes_{nullptr};

    uint32_t* sq_head_{nullptr};
    uint32_t* sq_tail_{nullptr};
    uint32_t sq_mask_{0};
    uint32_t* sq_array_{nullptr};

    uint32_t* cq_head_{nullptr};
    uint32_t* cq_tail_{nullptr};
    uint32_t cq_mask_{0};

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> total_bytes_echoed_{0};
};

} // namespace sys::net
