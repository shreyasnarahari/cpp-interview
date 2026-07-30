#include <iostream>
#include <chrono>
#include <vector>
#include <numeric>

// -------------------------------------------------------------
// 1. Dynamic Polymorphism (Virtual Table Dispatch)
// -------------------------------------------------------------
class VirtualBase {
public:
    virtual ~VirtualBase() = default;
    virtual double compute(double val) const = 0;
};

class VirtualDerived : public VirtualBase {
public:
    double compute(double val) const override {
        return val * 1.0001 + 0.5;
    }
};

// -------------------------------------------------------------
// 2. Static Polymorphism (Curiously Recurring Template Pattern)
// -------------------------------------------------------------
template <typename Derived>
class CRTPBase {
public:
    double compute(double val) const {
        return static_cast<const Derived*>(this)->compute_impl(val);
    }
};

class CRTPDerived : public CRTPBase<CRTPDerived> {
public:
    double compute_impl(double val) const {
        return val * 1.0001 + 0.5;
    }
};

// Benchmarking runner
int main() {
    constexpr size_t ITERATIONS = 50'000'000;

    // Benchmarking Virtual Dispatch
    {
        VirtualDerived derived;
        const VirtualBase* base_ptr = &derived;
        double sum = 0.0;

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ITERATIONS; ++i) {
            sum += base_ptr->compute(static_cast<double>(i));
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;

        std::cout << "[Virtual Dispatch] Time: " << duration.count() << " ms (sum: " << sum << ")\n";
    }

    // Benchmarking CRTP Static Dispatch
    {
        CRTPDerived crtp_obj;
        double sum = 0.0;

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ITERATIONS; ++i) {
            sum += crtp_obj.compute(static_cast<double>(i));
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;

        std::cout << "[CRTP Static Dispatch] Time: " << duration.count() << " ms (sum: " << sum << ")\n";
    }

    return 0;
}
