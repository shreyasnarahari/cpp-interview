#include "crash_probe.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cassert>

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 07: Crash Debugger Probe Verification Test\n";
    std::cout << "=======================================================\n\n";

    const char* dump_file = "/tmp/test_crash_probe.json";
    ::unlink(dump_file);

    pid_t pid = ::fork();
    assert(pid >= 0);

    if (pid == 0) {
        // Child Process: Install probe and trigger intentional SEGV
        assert(sys::debug::CrashProbe::install(dump_file));

        std::cout << "  [Child] Triggering intentional NULL pointer dereference...\n";
        volatile int* null_ptr = nullptr;
        *null_ptr = 42; // Triggers SIGSEGV -> CrashProbe handler
        ::_exit(0);
    }

    int status;
    ::waitpid(pid, &status, 0);

    std::cout << "  [Parent] Child process terminated with status: " << status << "\n";

    // Verify dump file was written
    std::ifstream ifs(dump_file);
    assert(ifs.is_open() && "Crash dump JSON file was not created!");

    std::stringstream ss;
    ss << ifs.rdbuf();
    const std::string content = ss.str();

    std::cout << "  [Parent] Read Crash Dump JSON:\n" << content << "\n";

    assert(content.find("\"signal_name\": \"SIGSEGV\"") != std::string::npos);
    assert(content.find("\"fault_address\": \"0x0000000000000000\"") != std::string::npos);
    assert(content.find("\"rip\":") != std::string::npos);
    assert(content.find("\"rsp\":") != std::string::npos);

    ::unlink(dump_file);
    std::cout << "\n>>> ALL CRASH DEBUGGER PROBE TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
