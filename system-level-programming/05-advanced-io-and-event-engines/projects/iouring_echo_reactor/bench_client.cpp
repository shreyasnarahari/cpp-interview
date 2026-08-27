#include "../../../common/rdtsc_timer.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <atomic>
#include <iomanip>
#include <cassert>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

void client_benchmark_worker(uint16_t port, 
                             size_t requests_per_client, 
                             std::atomic<uint64_t>& total_requests,
                             std::atomic<uint64_t>& total_bytes) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    ::inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (::connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
        ::close(sock);
        return;
    }

    const std::string payload = "BENCH_PING_1234567890_PONG";
    char recv_buf[256];

    for (size_t i = 0; i < requests_per_client; ++i) {
        ssize_t written = ::write(sock, payload.data(), payload.size());
        if (written <= 0) break;

        ssize_t read_bytes = ::read(sock, recv_buf, sizeof(recv_buf));
        if (read_bytes <= 0) break;

        total_requests.fetch_add(1, std::memory_order_relaxed);
        total_bytes.fetch_add(static_cast<uint64_t>(read_bytes), std::memory_order_relaxed);
    }

    ::close(sock);
}

int main(int argc, char* argv[]) {
    uint16_t port = 8081;
    size_t num_clients = 50;
    size_t reqs_per_client = 1000;

    if (argc > 1) port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (argc > 2) num_clients = static_cast<size_t>(std::atoi(argv[2]));
    if (argc > 3) reqs_per_client = static_cast<size_t>(std::atoi(argv[3]));

    const size_t total_expected = num_clients * reqs_per_client;

    std::cout << "========================================================================\n";
    std::cout << " High-Concurrency Client Benchmark for Async TCP Server\n";
    std::cout << " Port: " << port << " | Concurrent Clients: " << num_clients 
              << " | Total Requests: " << total_expected << "\n";
    std::cout << "========================================================================\n\n";

    sys::perf::TscCalibrator::instance().calibrate(20);

    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> total_bytes{0};

    std::vector<std::thread> threads;
    threads.reserve(num_clients);

    const uint64_t t0 = sys::perf::HardwareCycleCounter::start_cycles();

    for (size_t i = 0; i < num_clients; ++i) {
        threads.emplace_back(client_benchmark_worker, port, reqs_per_client, 
                             std::ref(total_requests), std::ref(total_bytes));
    }

    for (auto& t : threads) {
        t.join();
    }

    const uint64_t t1 = sys::perf::HardwareCycleCounter::end_cycles();

    const double elapsed_ns = sys::perf::TscCalibrator::instance().cycles_to_ns(t1 - t0);
    const double elapsed_sec = elapsed_ns / 1e9;
    const double reqs_per_sec = static_cast<double>(total_requests.load()) / elapsed_sec;

    std::cout << "------------------------------------------------------------------------\n"
              << " Completed Requests: " << total_requests.load() << " / " << total_expected << "\n"
              << " Elapsed Time:       " << std::fixed << std::setprecision(2) << (elapsed_ns / 1e6) << " ms\n"
              << " Request Throughput: " << std::fixed << std::setprecision(2) << reqs_per_sec << " reqs/sec\n"
              << " Data Transferred:   " << std::fixed << std::setprecision(2) << (static_cast<double>(total_bytes.load()) / (1024.0 * 1024.0)) << " MB\n"
              << "------------------------------------------------------------------------\n\n";

    return 0;
}
