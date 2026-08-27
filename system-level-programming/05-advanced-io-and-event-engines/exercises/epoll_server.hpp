#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <vector>
#include <iostream>
#include <atomic>
#include <thread>

namespace sys::net {

class EpollEchoServer {
public:
    static constexpr size_t MAX_EVENTS = 1024;
    static constexpr size_t BUFFER_SIZE = 4096;

    explicit EpollEchoServer(uint16_t port) : port_(port) {}

    ~EpollEchoServer() {
        stop();
    }

    EpollEchoServer(const EpollEchoServer&) = delete;
    EpollEchoServer& operator=(const EpollEchoServer&) = delete;

    bool init() {
        listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
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

        epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ < 0) {
            std::cerr << "Failed to create epoll instance: " << std::strerror(errno) << "\n";
            ::close(listen_fd_);
            listen_fd_ = -1;
            return false;
        }

        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET; // Edge-Triggered
        ev.data.fd = listen_fd_;

        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
            std::cerr << "Failed to add listen_fd to epoll: " << std::strerror(errno) << "\n";
            ::close(epoll_fd_);
            ::close(listen_fd_);
            epoll_fd_ = -1;
            listen_fd_ = -1;
            return false;
        }

        return true;
    }

    void run() {
        running_.store(true, std::memory_order_release);
        std::vector<struct epoll_event> events(MAX_EVENTS);

        while (running_.load(std::memory_order_relaxed)) {
            const int n = ::epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, 50);
            if (n < 0) {
                if (errno == EINTR) continue;
                break;
            }

            for (int i = 0; i < n; ++i) {
                const int fd = events[i].data.fd;
                const uint32_t evs = events[i].events;

                if (evs & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    close_client(fd);
                    continue;
                }

                if (fd == listen_fd_) {
                    handle_accept();
                } else if (evs & EPOLLIN) {
                    handle_read(fd);
                }
            }
        }
    }

    void stop() {
        running_.store(false, std::memory_order_release);
        if (epoll_fd_ >= 0) {
            ::close(epoll_fd_);
            epoll_fd_ = -1;
        }
        if (listen_fd_ >= 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
    }

    [[nodiscard]] uint16_t get_port() const noexcept { return port_; }
    [[nodiscard]] uint64_t get_total_bytes_echoed() const noexcept { return total_bytes_echoed_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t get_total_connections() const noexcept { return total_connections_.load(std::memory_order_relaxed); }

private:
    void handle_accept() {
        // Edge-Triggered requirement: Loop accept until EAGAIN
        for (;;) {
            struct sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            const int client_fd = ::accept4(listen_fd_, 
                                            reinterpret_cast<struct sockaddr*>(&client_addr), 
                                            &client_len, 
                                            SOCK_NONBLOCK | SOCK_CLOEXEC);

            if (client_fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break; // All pending connections accepted
                }
                break;
            }

            struct epoll_event ev{};
            ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
            ev.data.fd = client_fd;

            if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                ::close(client_fd);
            } else {
                total_connections_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    void handle_read(int client_fd) {
        char buffer[BUFFER_SIZE];

        // Edge-Triggered requirement: Drain receive buffer completely until EAGAIN
        for (;;) {
            const ssize_t bytes_read = ::read(client_fd, buffer, sizeof(buffer));

            if (bytes_read > 0) {
                total_bytes_echoed_.fetch_add(static_cast<uint64_t>(bytes_read), std::memory_order_relaxed);
                // Echo bytes back to client
                ssize_t total_written = 0;
                while (total_written < bytes_read) {
                    const ssize_t written = ::write(client_fd, buffer + total_written, static_cast<size_t>(bytes_read - total_written));
                    if (written < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // Socket send buffer full, yield briefly
                            std::this_thread::yield();
                            continue;
                        }
                        close_client(client_fd);
                        return;
                    }
                    total_written += written;
                }
            } else if (bytes_read == 0) {
                // Client disconnected (EOF)
                close_client(client_fd);
                break;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Entire buffer drained
                    break;
                }
                // Read error
                close_client(client_fd);
                break;
            }
        }
    }

    void close_client(int client_fd) {
        ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, client_fd, nullptr);
        ::close(client_fd);
    }

    uint16_t port_;
    int listen_fd_{-1};
    int epoll_fd_{-1};
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> total_bytes_echoed_{0};
    std::atomic<uint64_t> total_connections_{0};
};

} // namespace sys::net
