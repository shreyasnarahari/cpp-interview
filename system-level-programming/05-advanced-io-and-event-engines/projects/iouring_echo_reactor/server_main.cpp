#include "iouring_reactor.hpp"
#include <iostream>
#include <csignal>

static sys::net::IoUringReactor* g_reactor = nullptr;

void signal_handler(int) {
    if (g_reactor) {
        std::cout << "\nShutting down io_uring server...\n";
        g_reactor->stop();
    }
}

int main(int argc, char* argv[]) {
    uint16_t port = 8081;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    std::cout << "========================================================================\n";
    std::cout << " High-Throughput Linux io_uring TCP Echo Reactor (C++20)\n";
    std::cout << " Port: " << port << " | Direct Kernel Ring Submission\n";
    std::cout << "========================================================================\n\n";

    sys::net::IoUringReactor reactor(port);
    g_reactor = &reactor;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    if (!reactor.init()) {
        std::cerr << "Failed to initialize io_uring reactor.\n";
        return 1;
    }

    std::cout << "Reactor running on port " << port << ". Press Ctrl+C to stop.\n";
    reactor.run();

    std::cout << "Total bytes echoed: " << reactor.get_total_bytes_echoed() << " bytes\n";
    return 0;
}
