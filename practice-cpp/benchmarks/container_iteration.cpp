#include <iostream>
#include <chrono>
#include <vector>
#include <list>
#include <deque>
#include <numeric>

// -------------------------------------------------------------
// Benchmark: Container Iteration Performance
// Demonstrates cache locality effects: vector vs list vs deque
// for sequential traversal of large datasets.
// -------------------------------------------------------------

static constexpr size_t NUM_ELEMENTS = 1'000'000;
static constexpr size_t NUM_TRIALS = 5;

template <typename T>
void do_not_optimize(T&& val) {
    asm volatile("" : : "g"(val) : "memory");
}

template <typename Container>
double benchmark_iteration(const char* label, const Container& c) {
    double best_ms = 1e9;

    for (size_t trial = 0; trial < NUM_TRIALS; ++trial) {
        long long sum = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (const auto& elem : c) {
            sum += elem;
        }

        auto end = std::chrono::high_resolution_clock::now();
        do_not_optimize(sum);

        std::chrono::duration<double, std::milli> duration = end - start;
        best_ms = std::min(best_ms, duration.count());
    }

    std::cout << "  [" << label << "] " << best_ms << " ms (best of " << NUM_TRIALS << ")\n";
    return best_ms;
}

int main() {
    std::cout << "=== Container Iteration Benchmark ===\n";
    std::cout << "Elements: " << NUM_ELEMENTS << "\n";
    std::cout << "Trials: " << NUM_TRIALS << " (best time reported)\n\n";

    // --- Build containers with same data ---
    std::cout << "Building containers...\n";

    std::vector<int> vec(NUM_ELEMENTS);
    std::iota(vec.begin(), vec.end(), 0);  // fill with 0, 1, 2, ...

    std::list<int> lst(vec.begin(), vec.end());
    std::deque<int> deq(vec.begin(), vec.end());

    std::cout << "Done.\n\n";

    // --- Sequential traversal ---
    std::cout << "1. Sequential traversal (sum all elements):\n";
    double vec_ms = benchmark_iteration("std::vector", vec);
    double deq_ms = benchmark_iteration("std::deque ", deq);
    double lst_ms = benchmark_iteration("std::list  ", lst);

    std::cout << "\n  Relative performance:\n";
    std::cout << "    vector: 1.00x (baseline)\n";
    std::cout << "    deque:  " << deq_ms / vec_ms << "x\n";
    std::cout << "    list:   " << lst_ms / vec_ms << "x\n";

    // --- Random access (vector vs deque) ---
    std::cout << "\n2. Random access (sum elements at random indices):\n";

    // Generate random indices
    std::vector<size_t> indices(NUM_ELEMENTS);
    std::iota(indices.begin(), indices.end(), 0);
    // Simple shuffle using modular arithmetic (avoid <random> for simplicity)
    for (size_t i = NUM_ELEMENTS - 1; i > 0; --i) {
        size_t j = (i * 6364136223846793005ULL + 1) % (i + 1);
        std::swap(indices[i], indices[j]);
    }

    {
        double best_ms = 1e9;
        for (size_t trial = 0; trial < NUM_TRIALS; ++trial) {
            long long sum = 0;
            auto start = std::chrono::high_resolution_clock::now();
            for (size_t idx : indices) {
                sum += vec[idx];
            }
            auto end = std::chrono::high_resolution_clock::now();
            do_not_optimize(sum);
            std::chrono::duration<double, std::milli> duration = end - start;
            best_ms = std::min(best_ms, duration.count());
        }
        std::cout << "  [vector random] " << best_ms << " ms\n";
    }

    {
        double best_ms = 1e9;
        for (size_t trial = 0; trial < NUM_TRIALS; ++trial) {
            long long sum = 0;
            auto start = std::chrono::high_resolution_clock::now();
            for (size_t idx : indices) {
                sum += deq[idx];
            }
            auto end = std::chrono::high_resolution_clock::now();
            do_not_optimize(sum);
            std::chrono::duration<double, std::milli> duration = end - start;
            best_ms = std::min(best_ms, duration.count());
        }
        std::cout << "  [deque random]  " << best_ms << " ms\n";
    }

    std::cout << "\nKey takeaway: std::vector dominates sequential access due to\n"
              << "contiguous memory layout — the hardware prefetcher loads the\n"
              << "next cache line before you need it. std::list suffers ~5-50x\n"
              << "slowdown from random pointer chasing (cache miss per node).\n";

    return 0;
}
