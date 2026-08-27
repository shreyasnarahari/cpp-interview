#pragma once

#include <cstdint>
#include <cstddef>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <atomic>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#include <x86intrin.h>
#include <cpuid.h>
#endif

namespace sys::perf {

/**
 * @brief Hardware instruction-level cycle counters with memory fence serialization.
 * 
 * To measure pure execution latency without compiler or out-of-order CPU pipeline
 * reordering skewing results, LFENCE / CPUID serializing instructions must enclose
 * the RDTSC / RDTSCP reads.
 */
class HardwareCycleCounter {
public:
    /**
     * @brief Serializes the pipeline and captures the starting TSC value.
     */
    [[nodiscard]] static inline uint64_t start_cycles() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        _mm_lfence();
        uint64_t tsc = __builtin_ia32_rdtsc();
        _mm_lfence();
        return tsc;
#else
        return static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
#endif
    }

    /**
     * @brief Waits for previous instructions to execute, reads TSC, and serializes.
     */
    [[nodiscard]] static inline uint64_t end_cycles() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        unsigned int aux;
        uint64_t tsc = __builtin_ia32_rdtscp(&aux);
        _mm_lfence();
        return tsc;
#else
        return static_cast<uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
#endif
    }

    /**
     * @brief Heavy pipeline flush using CPUID prior to RDTSC.
     */
    [[nodiscard]] static inline uint64_t start_cycles_cpuid() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
        unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
        __cpuid(0, eax, ebx, ecx, edx);
        return __builtin_ia32_rdtsc();
#else
        return start_cycles();
#endif
    }
};

/**
 * @brief Calibrates and maintains the CPU invariant TSC frequency.
 */
class TscCalibrator {
public:
    static TscCalibrator& instance() noexcept {
        static TscCalibrator calibrator;
        return calibrator;
    }

    [[nodiscard]] double get_tsc_freq_ghz() const noexcept {
        return tsc_freq_ghz_;
    }

    [[nodiscard]] double get_cycles_per_ns() const noexcept {
        return cycles_per_ns_;
    }

    [[nodiscard]] double get_ns_per_cycle() const noexcept {
        return ns_per_cycle_;
    }

    [[nodiscard]] double cycles_to_ns(uint64_t cycles) const noexcept {
        return static_cast<double>(cycles) * ns_per_cycle_;
    }

    [[nodiscard]] uint64_t ns_to_cycles(double ns) const noexcept {
        return static_cast<uint64_t>(ns * cycles_per_ns_);
    }

    /**
     * @brief Forces recalibration over a specified duration in milliseconds.
     */
    void calibrate(uint32_t sample_period_ms = 50) noexcept {
        const auto sleep_duration = std::chrono::milliseconds(sample_period_ms);

        const auto start_time = std::chrono::steady_clock::now();
        const uint64_t start_tsc = HardwareCycleCounter::start_cycles();

        std::this_thread::sleep_for(sleep_duration);

        const uint64_t end_tsc = HardwareCycleCounter::end_cycles();
        const auto end_time = std::chrono::steady_clock::now();

        const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time).count();

        const uint64_t elapsed_cycles = end_tsc - start_tsc;

        if (elapsed_ns > 0) {
            cycles_per_ns_ = static_cast<double>(elapsed_cycles) / static_cast<double>(elapsed_ns);
            ns_per_cycle_ = 1.0 / cycles_per_ns_;
            tsc_freq_ghz_ = cycles_per_ns_;
        }
    }

private:
    TscCalibrator() noexcept {
        calibrate(20);
    }

    double cycles_per_ns_{3.0};
    double ns_per_cycle_{1.0 / 3.0};
    double tsc_freq_ghz_{3.0};
};

/**
 * @brief RAII high-precision execution timer.
 */
template <typename Callback>
class ScopedRdtscTimer {
public:
    explicit ScopedRdtscTimer(Callback&& callback) noexcept
        : callback_(std::forward<Callback>(callback)),
          start_cycles_(HardwareCycleCounter::start_cycles()) {}

    ~ScopedRdtscTimer() noexcept {
        const uint64_t end_cycles = HardwareCycleCounter::end_cycles();
        const uint64_t elapsed = (end_cycles >= start_cycles_) ? (end_cycles - start_cycles_) : 0;
        callback_(elapsed);
    }

    ScopedRdtscTimer(const ScopedRdtscTimer&) = delete;
    ScopedRdtscTimer& operator=(const ScopedRdtscTimer&) = delete;
    ScopedRdtscTimer(ScopedRdtscTimer&&) = delete;
    ScopedRdtscTimer& operator=(ScopedRdtscTimer&&) = delete;

private:
    Callback callback_;
    uint64_t start_cycles_;
};

/**
 * @brief Nanosecond and Cycle Latency Distribution Tracker.
 * 
 * Provides zero-allocation sample recording for production hotpaths
 * and calculates percentiles (min, max, p50, p90, p99, p99.9, p99.99).
 */
class LatencyTracker {
public:
    struct Statistics {
        size_t count{0};
        double min_ns{0.0};
        double max_ns{0.0};
        double mean_ns{0.0};
        double p50_ns{0.0};
        double p90_ns{0.0};
        double p99_ns{0.0};
        double p99_9_ns{0.0};
        double p99_99_ns{0.0};

        uint64_t min_cycles{0};
        uint64_t max_cycles{0};
        uint64_t mean_cycles{0};
        uint64_t p50_cycles{0};
        uint64_t p90_cycles{0};
        uint64_t p99_cycles{0};
        uint64_t p99_9_cycles{0};
        uint64_t p99_99_cycles{0};
    };

    explicit LatencyTracker(size_t max_samples = 1'000'000) {
        samples_.reserve(max_samples);
    }

    /**
     * @brief Record a cycle duration measurement (no dynamic allocation if within reserved capacity).
     */
    inline void record_cycles(uint64_t cycles) noexcept {
        if (samples_.size() < samples_.capacity()) {
            samples_.push_back(cycles);
        }
    }

    /**
     * @brief Record an explicit nanosecond duration.
     */
    inline void record_ns(double ns) noexcept {
        record_cycles(TscCalibrator::instance().ns_to_cycles(ns));
    }

    void reset() noexcept {
        samples_.clear();
    }

    [[nodiscard]] size_t size() const noexcept {
        return samples_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return samples_.empty();
    }

    /**
     * @brief Computes comprehensive statistical distribution.
     */
    [[nodiscard]] Statistics compute_stats() const {
        if (samples_.empty()) {
            return Statistics{};
        }

        std::vector<uint64_t> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());

        const size_t n = sorted.size();
        const auto& cal = TscCalibrator::instance();

        const uint64_t sum_cycles = std::accumulate(sorted.begin(), sorted.end(), uint64_t{0});
        const uint64_t mean_cycles = sum_cycles / n;

        auto get_percentile = [&](double p) -> uint64_t {
            if (p <= 0.0) return sorted.front();
            if (p >= 100.0) return sorted.back();
            const double rank = (p / 100.0) * static_cast<double>(n - 1);
            const size_t idx = static_cast<size_t>(std::round(rank));
            return sorted[std::min(idx, n - 1)];
        };

        Statistics stats;
        stats.count = n;

        stats.min_cycles = sorted.front();
        stats.max_cycles = sorted.back();
        stats.mean_cycles = mean_cycles;
        stats.p50_cycles = get_percentile(50.0);
        stats.p90_cycles = get_percentile(90.0);
        stats.p99_cycles = get_percentile(99.0);
        stats.p99_9_cycles = get_percentile(99.9);
        stats.p99_99_cycles = get_percentile(99.99);

        stats.min_ns = cal.cycles_to_ns(stats.min_cycles);
        stats.max_ns = cal.cycles_to_ns(stats.max_cycles);
        stats.mean_ns = cal.cycles_to_ns(stats.mean_cycles);
        stats.p50_ns = cal.cycles_to_ns(stats.p50_cycles);
        stats.p90_ns = cal.cycles_to_ns(stats.p90_cycles);
        stats.p99_ns = cal.cycles_to_ns(stats.p99_cycles);
        stats.p99_9_ns = cal.cycles_to_ns(stats.p99_9_cycles);
        stats.p99_99_ns = cal.cycles_to_ns(stats.p99_99_cycles);

        return stats;
    }

    /**
     * @brief Prints a formatted ASCII latency distribution table.
     */
    void print_summary(std::string_view label = "Benchmark", std::ostream& os = std::cout) const {
        const auto s = compute_stats();
        if (s.count == 0) {
            os << "No samples recorded for " << label << "\n";
            return;
        }

        os << "\n========================================================\n"
           << " Latency Profile: " << label << " (Samples: " << s.count << ")\n"
           << " CPU TSC Frequency: " << std::fixed << std::setprecision(3) 
           << TscCalibrator::instance().get_tsc_freq_ghz() << " GHz\n"
           << "--------------------------------------------------------\n"
           << " Metric        Cycles            Nanoseconds\n"
           << "--------------------------------------------------------\n"
           << " Min:          " << std::setw(12) << s.min_cycles << "     " << std::setw(10) << std::setprecision(2) << s.min_ns << " ns\n"
           << " Mean:         " << std::setw(12) << s.mean_cycles << "     " << std::setw(10) << std::setprecision(2) << s.mean_ns << " ns\n"
           << " P50 (Median): " << std::setw(12) << s.p50_cycles << "     " << std::setw(10) << std::setprecision(2) << s.p50_ns << " ns\n"
           << " P90:          " << std::setw(12) << s.p90_cycles << "     " << std::setw(10) << std::setprecision(2) << s.p90_ns << " ns\n"
           << " P99:          " << std::setw(12) << s.p99_cycles << "     " << std::setw(10) << std::setprecision(2) << s.p99_ns << " ns\n"
           << " P99.9:        " << std::setw(12) << s.p99_9_cycles << "     " << std::setw(10) << std::setprecision(2) << s.p99_9_ns << " ns\n"
           << " P99.99:       " << std::setw(12) << s.p99_99_cycles << "     " << std::setw(10) << std::setprecision(2) << s.p99_99_ns << " ns\n"
           << " Max:          " << std::setw(12) << s.max_cycles << "     " << std::setw(10) << std::setprecision(2) << s.max_ns << " ns\n"
           << "========================================================\n\n";
    }

private:
    std::vector<uint64_t> samples_;
};

} // namespace sys::perf
