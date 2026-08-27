#include "iouring_reactor.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

void client_test_worker(uint16_t port, size_t id) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    int res = ::connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr));
    assert(res == 0);

    std::string payload = "IO_URING_ECHO_MSG_ID_" + std::to_string(id);
    ssize_t written = ::write(sock, payload.data(), payload.size());
    assert(written == static_cast<ssize_t>(payload.size()));

    char recv_buf[256]{0};
    ssize_t read_bytes = ::read(sock, recv_buf, sizeof(recv_buf) - 1);
    assert(read_bytes == static_cast<ssize_t>(payload.size()));
    assert(std::string_view(recv_buf, static_cast<size_t>(read_bytes)) == payload);

    ::close(sock);
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 05: Linux io_uring TCP Echo Reactor Test\n";
    std::cout << "=======================================================\n\n";

    constexpr uint16_t PORT = 19081;
    sys::net::IoUringReactor reactor(PORT);

    if (!reactor.init()) {
        std::cout << "  [NOTE] io_uring not supported on this kernel/environment. Skipping runtime test.\n";
        return 0;
    }

    std::thread server_thread([&] {
        reactor.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    constexpr size_t CLIENTS = 5;
    std::vector<std::thread> threads;
    threads.reserve(CLIENTS);

    for (size_t i = 0; i < CLIENTS; ++i) {
        threads.emplace_back(client_test_worker, PORT, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    reactor.stop();
    server_thread.join();

    std::cout << "  io_uring echoed: " << reactor.get_total_bytes_echoed() << " bytes.\n";
    assert(reactor.get_total_bytes_echoed() > 0);

    std::cout << "\n>>> ALL IO_URING TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
