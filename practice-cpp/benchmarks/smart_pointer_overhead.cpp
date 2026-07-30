#include <iostream>
#include <chrono>
#include <memory>
#include <vector>

// -------------------------------------------------------------
// Benchmark: Smart Pointer Overhead
// Compares raw pointer, unique_ptr, and shared_ptr
// for creation, access, and destruction patterns.
// -------------------------------------------------------------

static constexpr size_t ITERATIONS = 10'000'000;

struct Widget {
    double value = 0.0;
    double compute() const { return value * 1.0001 + 0.5; }
};

// Prevent compiler from optimizing away the result
template <typename T>
void do_not_optimize(T&& val) {
    asm volatile("" : : "g"(val) : "memory");
}

template <typename Func>
double benchmark(const char* label, Func&& fn) {
    auto start = std::chrono::high_resolution_clock::now();
    fn();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    std::cout << "  [" << label << "] " << duration.count() << " ms\n";
    return duration.count();
}

int main() {
    std::cout << "=== Smart Pointer Overhead Benchmark ===\n";
    std::cout << "Iterations: " << ITERATIONS << "\n\n";

    // --- 1. Creation + Destruction ---
    std::cout << "1. Creation + Destruction:\n";

    benchmark("raw new/delete", [] {
        for (size_t i = 0; i < ITERATIONS; ++i) {
            Widget* w = new Widget{static_cast<double>(i)};
            do_not_optimize(w->value);
            delete w;
        }
    });

    benchmark("make_unique", [] {
        for (size_t i = 0; i < ITERATIONS; ++i) {
            auto w = std::make_unique<Widget>(Widget{static_cast<double>(i)});
            do_not_optimize(w->value);
        }
    });

    benchmark("make_shared", [] {
        for (size_t i = 0; i < ITERATIONS; ++i) {
            auto w = std::make_shared<Widget>(Widget{static_cast<double>(i)});
            do_not_optimize(w->value);
        }
    });

    // --- 2. Access through pointer ---
    std::cout << "\n2. Access through pointer (" << ITERATIONS << " calls):\n";

    {
        Widget* raw = new Widget{42.0};
        benchmark("raw pointer access", [&] {
            double sum = 0.0;
            for (size_t i = 0; i < ITERATIONS; ++i) {
                sum += raw->compute();
            }
            do_not_optimize(sum);
        });
        delete raw;
    }

    {
        auto uptr = std::make_unique<Widget>(Widget{42.0});
        benchmark("unique_ptr access", [&] {
            double sum = 0.0;
            for (size_t i = 0; i < ITERATIONS; ++i) {
                sum += uptr->compute();
            }
            do_not_optimize(sum);
        });
    }

    {
        auto sptr = std::make_shared<Widget>(Widget{42.0});
        benchmark("shared_ptr access", [&] {
            double sum = 0.0;
            for (size_t i = 0; i < ITERATIONS; ++i) {
                sum += sptr->compute();
            }
            do_not_optimize(sum);
        });
    }

    // --- 3. Copy overhead (shared_ptr only) ---
    std::cout << "\n3. shared_ptr copy (atomic refcount increment):\n";

    {
        auto original = std::make_shared<Widget>(Widget{42.0});
        benchmark("shared_ptr copy", [&] {
            for (size_t i = 0; i < ITERATIONS; ++i) {
                auto copy = original;  // atomic increment + decrement
                do_not_optimize(copy.get());
            }
        });
    }

    std::cout << "\nKey takeaway: unique_ptr access has ZERO overhead vs raw pointer.\n"
              << "shared_ptr copies are expensive due to atomic refcount operations.\n";

    return 0;
}
