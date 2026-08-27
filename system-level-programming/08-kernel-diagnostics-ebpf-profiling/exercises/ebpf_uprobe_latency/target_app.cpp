#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <random>

// Marking functions with extern "C" and noinline ensures stable symbol names for Uprobes
extern "C" {

__attribute__((noinline)) uint64_t process_order_matching(uint64_t order_id, double price, uint32_t qty) {
    // Simulate hotpath order matching computation
    uint64_t hash = order_id ^ static_cast<uint64_t>(price * 100.0);
    for (uint32_t i = 0; i < qty; ++i) {
        hash = (hash * 6364136223846793005ULL) + 1;
    }
    return hash;
}

__attribute__((noinline)) double calculate_risk_exposure(double base_val, double volatility) {
    double res = base_val;
    for (int i = 0; i < 50; ++i) {
        res += (res * volatility * 0.01);
    }
    return res;
}

} // extern "C"

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Target C++ Application for eBPF Uprobe Tracing\n";
    std::cout << " PID: " << ::getpid() << " | Symbols: process_order_matching, calculate_risk_exposure\n";
    std::cout << "========================================================================\n\n";

    std::mt19937_64 rng(42);
    std::uniform_int_distribution<uint64_t> id_dist(1000, 9999);
    std::uniform_real_distribution<double> price_dist(100.0, 500.0);
    std::uniform_int_distribution<uint32_t> qty_dist(10, 100);

    for (int iteration = 1; iteration <= 1000; ++iteration) {
        const uint64_t oid = id_dist(rng);
        const double price = price_dist(rng);
        const uint32_t qty = qty_dist(rng);

        const uint64_t match_res = process_order_matching(oid, price, qty);
        const double risk = calculate_risk_exposure(static_cast<double>(match_res % 10000), 0.05);

        if (iteration % 200 == 0) {
            std::cout << "  [Iteration " << iteration << "] Processed Order: " << oid 
                      << " | Match Hash: " << match_res << " | Risk: " << risk << "\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::cout << "\nTarget workload complete.\n";
    return 0;
}
