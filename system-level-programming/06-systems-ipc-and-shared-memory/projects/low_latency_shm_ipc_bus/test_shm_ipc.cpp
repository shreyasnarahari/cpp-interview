#include "shm_ipc_bus.hpp"
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cassert>
#include <string>
#include <immintrin.h>

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 06: POSIX Shared Memory IPC Multi-Process Test\n";
    std::cout << "=======================================================\n\n";

    const std::string shm_name = "/shm_test_bus";
    constexpr size_t CAPACITY = 4096;
    constexpr size_t MESSAGES = 50'000;

    // Create shared memory in parent before fork
    auto bus_creator = sys::ipc::ShmIpcBus::create(shm_name, CAPACITY);

    pid_t pid = ::fork();
    assert(pid >= 0);

    if (pid == 0) {
        // Child Process: Consumer
        auto consumer = sys::ipc::ShmIpcBus::attach(shm_name, CAPACITY);
        size_t received = 0;
        uint64_t expected_id = 1;

        while (received < MESSAGES) {
            sys::ipc::IpcMessage msg;
            if (consumer.consume(msg)) {
                assert(msg.msg_id == expected_id);
                std::string payload_str(msg.payload, msg.length);
                assert(payload_str == "TICK_DATA_MSG_" + std::to_string(expected_id));
                ++expected_id;
                ++received;
            } else {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
        }

        std::cout << "  Child Consumer successfully received and verified " << received << " messages.\n";
        ::_exit(0);
    } else {
        // Parent Process: Producer
        for (uint64_t i = 1; i <= MESSAGES; ++i) {
            std::string payload = "TICK_DATA_MSG_" + std::to_string(i);
            while (!bus_creator.publish(i, i * 1000, payload)) {
                #if defined(__x86_64__) || defined(_M_X64)
                _mm_pause();
                #endif
            }
        }

        int status;
        ::waitpid(pid, &status, 0);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }

    std::cout << "\n>>> ALL SHM IPC BUS TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
