#include "epoll_server.hpp"
#include <iostream>
#include <csignal>

static sys::net::EpollEchoServer* g_server = nullptr;

void signal_handler(int) {
    if (g_server) {
        std::cout << "\nShutting down server gracefully...\n";
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    std::cout << "========================================================================\n";
    std::cout << " Non-Blocking Edge-Triggered epoll TCP Echo Server (C++20)\n";
    std::cout << " Port: " << port << " | EPOLLET Enabled\n";
    std::cout << "========================================================================\n\n";

    sys::net::EpollEchoServer server(port);
    g_server = &server;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!server.init()) {
        std::cerr << "Failed to initialize server.\n";
        return 1;
    }

    std::cout << "Server listening on port " << port << ". Press Ctrl+C to exit.\n";
    server.run();

    std::cout << "Total connections handled: " << server.get_total_connections() << "\n";
    std::cout << "Total bytes echoed:        " << server.get_total_bytes_echoed() << " bytes\n";
    return 0;
}
