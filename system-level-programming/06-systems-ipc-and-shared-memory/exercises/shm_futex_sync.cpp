#include "futex_mutex.hpp"
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cassert>

struct SharedCounterData {
    sys::ipc::FutexMutex mutex;
    uint64_t counter{0};
};

int main() {
    std::cout << "========================================================================\n";
    std::cout << " Multi-Process Shared Memory Synchronization with Linux Futex (C++20)\n";
    std::cout << "========================================================================\n\n";

    const char* shm_name = "/shm_futex_demo";
    const size_t shm_size = sizeof(SharedCounterData);

    // Create shared memory object
    int fd = ::shm_open(shm_name, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) {
        std::cerr << "shm_open failed!\n";
        return 1;
    }

    if (::ftruncate(fd, static_cast<off_t>(shm_size)) < 0) {
        std::cerr << "ftruncate failed!\n";
        return 1;
    }

    auto* data = static_cast<SharedCounterData*>(::mmap(
        nullptr, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));

    if (data == MAP_FAILED) {
        std::cerr << "mmap failed!\n";
        return 1;
    }

    // Initialize shared struct in-place
    new (data) SharedCounterData();

    constexpr size_t NUM_PROCESSES = 4;
    constexpr size_t INCREMENTS_PER_PROCESS = 250'000;
    const size_t expected_total = NUM_PROCESSES * INCREMENTS_PER_PROCESS;

    std::cout << "Forking " << NUM_PROCESSES << " processes (each incrementing " 
              << INCREMENTS_PER_PROCESS << " times under FutexMutex)...\n";

    for (size_t p = 0; p < NUM_PROCESSES; ++p) {
        pid_t pid = ::fork();
        if (pid == 0) {
            // Child process
            for (size_t i = 0; i < INCREMENTS_PER_PROCESS; ++i) {
                sys::ipc::ScopedFutexLock lock(data->mutex);
                data->counter += 1;
            }
            ::munmap(data, shm_size);
            ::close(fd);
            ::_exit(0);
        }
    }

    // Parent waits for all children
    for (size_t p = 0; p < NUM_PROCESSES; ++p) {
        int status;
        ::wait(&status);
    }

    std::cout << "All child processes finished.\n"
              << "Final Shared Counter Value: " << data->counter << " (Expected: " << expected_total << ")\n";

    assert(data->counter == expected_total);
    std::cout << ">>> Multi-Process Futex Synchronization Succeeded with Zero Data Races! <<<\n";

    ::munmap(data, shm_size);
    ::close(fd);
    ::shm_unlink(shm_name);
    return 0;
}
