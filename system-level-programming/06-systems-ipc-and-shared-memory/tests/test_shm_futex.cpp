#include "../exercises/futex_mutex.hpp"
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cassert>

struct SharedSyncBlock {
    sys::ipc::FutexMutex mutex;
    uint64_t counter{0};
};

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 06: Cross-Process Futex Mutex Stress Test\n";
    std::cout << "=======================================================\n\n";

    const char* shm_name = "/shm_test_futex";
    const size_t shm_sz = sizeof(SharedSyncBlock);

    int fd = ::shm_open(shm_name, O_CREAT | O_RDWR | O_TRUNC, 0666);
    assert(fd >= 0);
    assert(::ftruncate(fd, static_cast<off_t>(shm_sz)) == 0);

    auto* block = static_cast<SharedSyncBlock*>(::mmap(
        nullptr, shm_sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    assert(block != MAP_FAILED);

    new (block) SharedSyncBlock();

    constexpr size_t FORK_COUNT = 8;
    constexpr size_t INCS = 100'000;
    const uint64_t expected = FORK_COUNT * INCS;

    std::cout << "Launching " << FORK_COUNT << " processes (total increments: " << expected << ")...\n";

    for (size_t i = 0; i < FORK_COUNT; ++i) {
        pid_t pid = ::fork();
        assert(pid >= 0);
        if (pid == 0) {
            for (size_t k = 0; k < INCS; ++k) {
                sys::ipc::ScopedFutexLock lock(block->mutex);
                block->counter += 1;
            }
            ::munmap(block, shm_sz);
            ::close(fd);
            ::_exit(0);
        }
    }

    for (size_t i = 0; i < FORK_COUNT; ++i) {
        int status;
        ::wait(&status);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }

    std::cout << "  Counter value: " << block->counter << " (Expected: " << expected << ")\n";
    assert(block->counter == expected);

    ::munmap(block, shm_sz);
    ::close(fd);
    ::shm_unlink(shm_name);

    std::cout << "\n>>> ALL SHM FUTEX TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
