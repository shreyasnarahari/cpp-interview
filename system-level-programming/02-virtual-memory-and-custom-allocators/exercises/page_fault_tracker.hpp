#pragma once

#include <cstdint>
#include <cstddef>
#include <sys/resource.h>
#include <sys/mman.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

namespace sys::vm {

struct FaultStats {
    uint64_t minor_faults{0}; // Soft page faults (no disk I/O)
    uint64_t major_faults{0}; // Hard page faults (disk / swap I/O)
    uint64_t rss_kb{0};       // Resident Set Size in KB
};

class PageFaultTracker {
public:
    [[nodiscard]] static FaultStats get_current_stats() noexcept {
        struct rusage usage{};
        ::getrusage(RUSAGE_SELF, &usage);

        FaultStats stats;
        stats.minor_faults = static_cast<uint64_t>(usage.ru_minflt);
        stats.major_faults = static_cast<uint64_t>(usage.ru_majflt);
        stats.rss_kb = static_cast<uint64_t>(usage.ru_maxrss); // in KB on Linux
        return stats;
    }

    /**
     * @brief Scoped delta measurement for page faults.
     */
    class ScopedTracker {
    public:
        explicit ScopedTracker(std::string_view label, std::ostream& os = std::cout)
            : label_(label), os_(os), start_stats_(get_current_stats()) {}

        ~ScopedTracker() {
            const auto end_stats = get_current_stats();
            const uint64_t delta_min = (end_stats.minor_faults >= start_stats_.minor_faults)
                                       ? (end_stats.minor_faults - start_stats_.minor_faults) : 0;
            const uint64_t delta_maj = (end_stats.major_faults >= start_stats_.major_faults)
                                       ? (end_stats.major_faults - start_stats_.major_faults) : 0;

            os_ << "  [" << label_ << "] -> Minor Faults (Soft): +" 
                << delta_min << " | Major Faults (Hard): +" 
                << delta_maj << "\n";
        }

    private:
        std::string_view label_;
        std::ostream& os_;
        FaultStats start_stats_;
    };
};

} // namespace sys::vm
