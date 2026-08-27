#pragma once

#include <cstdint>
#include <cstddef>
#include <csignal>
#include <sys/ucontext.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

namespace sys::debug {

namespace detail {

    inline constexpr char HEX_MAP[] = "0123456789abcdef";

    inline char* write_str(char* dst, const char* str) noexcept {
        while (*str) {
            *dst++ = *str++;
        }
        return dst;
    }

    inline char* write_hex64(char* dst, uint64_t val) noexcept {
        *dst++ = '0';
        *dst++ = 'x';
        for (int i = 60; i >= 0; i -= 4) {
            *dst++ = HEX_MAP[(val >> i) & 0x0F];
        }
        return dst;
    }

    inline char* write_uint(char* dst, uint64_t val) noexcept {
        char temp[24];
        int idx = 0;
        if (val == 0) {
            temp[idx++] = '0';
        } else {
            while (val > 0) {
                temp[idx++] = static_cast<char>('0' + (val % 10));
                val /= 10;
            }
        }
        for (int i = idx - 1; i >= 0; --i) {
            *dst++ = temp[i];
        }
        return dst;
    }
}

class CrashProbe {
public:
    static inline char dump_path_[256]{"/tmp/crash_dump.json"};
    static inline void* alt_stack_mem_{nullptr};
    static constexpr size_t ALT_STACK_SIZE = 64 * 1024; // 64 KB

    static bool install(const char* output_json_path = "/tmp/crash_dump.json") noexcept {
        if (output_json_path) {
            size_t len = std::strlen(output_json_path);
            if (len < sizeof(dump_path_)) {
                std::memcpy(dump_path_, output_json_path, len + 1);
            }
        }

        // Allocate dedicated alternate signal stack
        alt_stack_mem_ = ::mmap(nullptr, ALT_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (alt_stack_mem_ == MAP_FAILED) {
            return false;
        }

        stack_t ss{};
        ss.ss_sp = alt_stack_mem_;
        ss.ss_size = ALT_STACK_SIZE;
        ss.ss_flags = 0;
        if (::sigaltstack(&ss, nullptr) < 0) {
            return false;
        }

        struct sigaction sa{};
        sa.sa_sigaction = &crash_handler;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER | SA_RESETHAND;
        ::sigemptyset(&sa.sa_mask);

        ::sigaction(SIGSEGV, &sa, nullptr);
        ::sigaction(SIGBUS,  &sa, nullptr);
        ::sigaction(SIGFPE,  &sa, nullptr);
        ::sigaction(SIGILL,  &sa, nullptr);
        ::sigaction(SIGABRT, &sa, nullptr);

        return true;
    }

private:
    static void crash_handler(int sig, siginfo_t* info, void* ucontext_raw) noexcept {
        auto* uc = static_cast<ucontext_t*>(ucontext_raw);

        // Extract CPU Registers (x86_64 gregs)
#if defined(__x86_64__) || defined(_M_X64)
        const auto* gregs = uc->uc_mcontext.gregs;
        const uint64_t rip = static_cast<uint64_t>(gregs[REG_RIP]);
        const uint64_t rsp = static_cast<uint64_t>(gregs[REG_RSP]);
        const uint64_t rbp = static_cast<uint64_t>(gregs[REG_RBP]);
        const uint64_t rax = static_cast<uint64_t>(gregs[REG_RAX]);
        const uint64_t rbx = static_cast<uint64_t>(gregs[REG_RBX]);
        const uint64_t rcx = static_cast<uint64_t>(gregs[REG_RCX]);
        const uint64_t rdx = static_cast<uint64_t>(gregs[REG_RDX]);
        const uint64_t rsi = static_cast<uint64_t>(gregs[REG_RSI]);
        const uint64_t rdi = static_cast<uint64_t>(gregs[REG_RDI]);
        const uint64_t r8  = static_cast<uint64_t>(gregs[REG_R8]);
        const uint64_t r9  = static_cast<uint64_t>(gregs[REG_R9]);
        const uint64_t r10 = static_cast<uint64_t>(gregs[REG_R10]);
        const uint64_t r11 = static_cast<uint64_t>(gregs[REG_R11]);
        const uint64_t r12 = static_cast<uint64_t>(gregs[REG_R12]);
        const uint64_t r13 = static_cast<uint64_t>(gregs[REG_R13]);
        const uint64_t r14 = static_cast<uint64_t>(gregs[REG_R14]);
        const uint64_t r15 = static_cast<uint64_t>(gregs[REG_R15]);
        const uint64_t efl = static_cast<uint64_t>(gregs[REG_EFL]);
#else
        const uint64_t rip = 0, rsp = 0, rbp = 0, rax = 0, rbx = 0, rcx = 0, rdx = 0, rsi = 0, rdi = 0;
        const uint64_t r8 = 0, r9 = 0, r10 = 0, r11 = 0, r12 = 0, r13 = 0, r14 = 0, r15 = 0, efl = 0;
#endif
        const uint64_t fault_addr = reinterpret_cast<uint64_t>(info->si_addr);

        // Build JSON Crash Dump Buffer
        char buf[2048];
        char* p = buf;

        p = detail::write_str(p, "{\n  \"crash_report\": {\n");
        p = detail::write_str(p, "    \"signal\": ");
        p = detail::write_uint(p, static_cast<uint64_t>(sig));
        p = detail::write_str(p, ",\n    \"signal_name\": \"");
        switch (sig) {
            case SIGSEGV: p = detail::write_str(p, "SIGSEGV"); break;
            case SIGBUS:  p = detail::write_str(p, "SIGBUS"); break;
            case SIGFPE:  p = detail::write_str(p, "SIGFPE"); break;
            case SIGILL:  p = detail::write_str(p, "SIGILL"); break;
            case SIGABRT: p = detail::write_str(p, "SIGABRT"); break;
            default:      p = detail::write_str(p, "UNKNOWN"); break;
        }
        p = detail::write_str(p, "\",\n    \"fault_address\": \"");
        p = detail::write_hex64(p, fault_addr);
        p = detail::write_str(p, "\",\n    \"registers\": {\n");

        p = detail::write_str(p, "      \"rip\": \""); p = detail::write_hex64(p, rip); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"rsp\": \""); p = detail::write_hex64(p, rsp); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"rbp\": \""); p = detail::write_hex64(p, rbp); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"rax\": \""); p = detail::write_hex64(p, rax); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"rbx\": \""); p = detail::write_hex64(p, rbx); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"rcx\": \""); p = detail::write_hex64(p, rcx); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"rdx\": \""); p = detail::write_hex64(p, rdx); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"rsi\": \""); p = detail::write_hex64(p, rsi); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"rdi\": \""); p = detail::write_hex64(p, rdi); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"r8\":  \""); p = detail::write_hex64(p, r8);  p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"r9\":  \""); p = detail::write_hex64(p, r9);  p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"r10\": \""); p = detail::write_hex64(p, r10); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"r11\": \""); p = detail::write_hex64(p, r11); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"r12\": \""); p = detail::write_hex64(p, r12); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"r13\": \""); p = detail::write_hex64(p, r13); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"r14\": \""); p = detail::write_hex64(p, r14); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"r15\": \""); p = detail::write_hex64(p, r15); p = detail::write_str(p, "\",\n");
        p = detail::write_str(p, "      \"efl\": \""); p = detail::write_hex64(p, efl); p = detail::write_str(p, "\"\n");

        p = detail::write_str(p, "    }\n  }\n}\n");

        const size_t total_len = static_cast<size_t>(p - buf);

        // Write directly to standard error
        (void)::write(STDERR_FILENO, buf, total_len);

        // Write to dump file
        int fd = ::open(dump_path_, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) {
            (void)::write(fd, buf, total_len);
            ::close(fd);
        }

        ::_exit(128 + sig);
    }
};

} // namespace sys::debug
