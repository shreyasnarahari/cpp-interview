#!/usr/bin/env python3
"""
eBPF Uprobe Latency Profiler using Python BCC.
Attaches to `process_order_matching` in `target_app` and computes latency histogram.
"""

import sys
import time

bpf_source = """
#include <uapi/linux/ptrace.h>

BPF_HASH(start_time, u32, u64);
BPF_HISTOGRAM(dist);

int trace_entry(struct pt_regs *ctx) {
    u32 pid = bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();
    start_time.update(&pid, &ts);
    return 0;
}

int trace_return(struct pt_regs *ctx) {
    u32 pid = bpf_get_current_pid_tgid();
    u64 *tsp = start_time.lookup(&pid);
    if (tsp != 0) {
        u64 delta = bpf_ktime_get_ns() - *tsp;
        dist.increment(bpf_log2l(delta / 1000)); // Microseconds histogram
        start_time.delete(&pid);
    }
    return 0;
}
"""

def main():
    binary_path = "./target_app"
    if len(sys.argv) > 1:
        binary_path = sys.argv[1]

    try:
        from bcc import BPF
    except ImportError:
        print("[NOTE] BCC (BPF Compiler Collection) is not installed on this system.")
        print(f"[INFO] To run eBPF uprobe tracing on Linux: sudo python3 {sys.argv[0]} <binary_path>")
        print("\nBPF C Source Code:")
        print(bpf_source)
        return

    b = BPF(text=bpf_source)
    b.attach_uprobe(name=binary_path, sym="process_order_matching", fn_name="trace_entry")
    b.attach_uretprobe(name=binary_path, sym="process_order_matching", fn_name="trace_return")

    print(f"Tracing `process_order_matching` in {binary_path}... Hit Ctrl-C to end.")
    try:
        while True:
            time.sleep(2)
    except KeyboardInterrupt:
        print("\nFunction Execution Latency Distribution (Microseconds):")
        b["dist"].print_log2_hist("usecs")

if __name__ == "__main__":
    main()
