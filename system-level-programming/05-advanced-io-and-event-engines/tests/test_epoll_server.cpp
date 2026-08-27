#include "../exercises/epoll_server.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

void run_client_worker(uint16_t port, size_t client_id, size_t msgs_per_client) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    // Connect to server
    int res = ::connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr));
    assert(res == 0);

    for (size_t m = 0; m < msgs_per_client; ++m) {
        std::string payload = "Client_" + std::to_string(client_id) + "_Msg_" + std::to_string(m);
        ssize_t written = ::write(sock, payload.data(), payload.size());
        assert(written == static_cast<ssize_t>(payload.size()));

        char recv_buf[256]{0};
        ssize_t bytes_read = ::read(sock, recv_buf, sizeof(recv_buf) - 1);
        assert(bytes_read == static_cast<ssize_t>(payload.size()));
        assert(std::string_view(recv_buf, static_cast<size_t>(bytes_read)) == payload);
    }

    ::close(sock);
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 05: Edge-Triggered epoll Multi-Client Echo Test\n";
    std::cout << "=======================================================\n\n";

    constexpr uint16_t PORT = 19080;
    constexpr size_t NUM_CLIENTS = 10;
    constexpr size_t MSGS_PER_CLIENT = 100;

    sys::net::EpollEchoServer server(PORT);
    assert(server.init());

    std::thread server_thread([&] {
        server.run();
    });

    // Give server a moment to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cout << "Spawning " << NUM_CLIENTS << " concurrent TCP clients (each sending " 
              << MSGS_PER_CLIENT << " messages)...\n";

    std::vector<std::thread> clients;
    clients.reserve(NUM_CLIENTS);

    for (size_t i = 0; i < NUM_CLIENTS; ++i) {
        clients.emplace_back(run_client_worker, PORT, i, MSGS_PER_CLIENT);
    }

    for (auto& t : clients) {
        t.join();
    }

    std::cout << "All clients finished echoing successfully.\n";

    server.stop();
    server_thread.join();

    std::cout << "Total connections handled: " << server.get_total_connections() << "\n";
    std::cout << "Total bytes echoed:        " << server.get_total_bytes_echoed() << "\n";

    assert(server.get_total_connections() == NUM_CLIENTS);
    assert(server.get_total_bytes_echoed() > 0);

    std::cout << "\n>>> ALL EPOLL SERVER TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
