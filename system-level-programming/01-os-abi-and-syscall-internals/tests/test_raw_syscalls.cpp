#include "../exercises/raw_syscalls.hpp"
#include <iostream>
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

void test_pid_syscall() {
    std::cout << "[Test 1] Testing sys_getpid() ...\n";
    int64_t raw_pid = sys::abi::sys_getpid();
    pid_t libc_pid = ::getpid();
    std::cout << "  Raw syscall PID: " << raw_pid << " | libc PID: " << libc_pid << "\n";
    assert(raw_pid == libc_pid);
    assert(raw_pid > 0);
}

void test_file_io_syscalls() {
    std::cout << "[Test 2] Testing sys_open, sys_write, sys_read, sys_close ...\n";
    const char* filename = "/tmp/raw_syscall_test.tmp";
    const char payload[] = "System V AMD64 ABI Low-Level Syscall Verification Data 123456789\n";
    const size_t payload_len = sizeof(payload) - 1;

    // Open/Create file
    int64_t fd = sys::abi::sys_open(filename, O_CREAT | O_RDWR | O_TRUNC, 0644);
    std::cout << "  Opened file fd: " << fd << "\n";
    assert(fd >= 0);

    // Write data
    int64_t written = sys::abi::sys_write(static_cast<int>(fd), payload, payload_len);
    std::cout << "  Bytes written: " << written << " (expected: " << payload_len << ")\n";
    assert(written == static_cast<int64_t>(payload_len));

    // Close
    int64_t close_ret = sys::abi::sys_close(static_cast<int>(fd));
    assert(close_ret == 0);

    // Reopen for reading
    int64_t read_fd = sys::abi::sys_open(filename, O_RDONLY, 0);
    assert(read_fd >= 0);

    char read_buffer[128]{0};
    int64_t bytes_read = sys::abi::sys_read(static_cast<int>(read_fd), read_buffer, sizeof(read_buffer) - 1);
    std::cout << "  Bytes read: " << bytes_read << "\n";
    assert(bytes_read == static_cast<int64_t>(payload_len));
    assert(std::memcmp(payload, read_buffer, payload_len) == 0);

    sys::abi::sys_close(static_cast<int>(read_fd));
    ::unlink(filename);
}

void test_mmap_syscalls() {
    std::cout << "[Test 3] Testing sys_mmap & sys_munmap anonymous page allocation ...\n";
    const size_t page_size = 4096;

    // Map anonymous private readable/writable memory
    void* mapped = sys::abi::sys_mmap(
        nullptr,
        page_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    std::cout << "  Mapped memory address: " << mapped << "\n";
    assert(mapped != nullptr && mapped != MAP_FAILED);

    // Write data to mapped memory
    auto* int_ptr = static_cast<uint64_t*>(mapped);
    for (size_t i = 0; i < page_size / sizeof(uint64_t); ++i) {
        int_ptr[i] = i * 42;
    }

    // Verify written data
    for (size_t i = 0; i < page_size / sizeof(uint64_t); ++i) {
        assert(int_ptr[i] == i * 42);
    }

    // Unmap
    int64_t unmap_ret = sys::abi::sys_munmap(mapped, page_size);
    assert(unmap_ret == 0);
    std::cout << "  Unmapped successfully.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 01: Raw Inline Assembly Syscall Tests\n";
    std::cout << "=======================================================\n\n";

    test_pid_syscall();
    test_file_io_syscalls();
    test_mmap_syscalls();

    std::cout << "\n>>> ALL RAW SYSCALL TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
