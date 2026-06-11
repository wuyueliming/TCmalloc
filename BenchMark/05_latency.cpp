#include "common.h"

// Test 05: Fixed-Size Alloc/Free Latency
// 测量不同固定大小下单次 alloc/free 的延迟

int main() {
    printf("================================================================\n");
    printf("  Test 05: Fixed-Size Alloc/Free Latency\n");
    printf("================================================================\n\n");

    print_system_info();
    warmup();

    printf("%-10s %-14s %10s %10s %10s\n",
           "Size", "Allocator", "Alloc(ns)", "Free(ns)", "Total(ns)");
    printf("%s\n", std::string(60, '-').c_str());

    SizeDist fixed_sizes[] = {
        {8, 8, "8B"},
        {32, 32, "32B"},
        {64, 64, "64B"},
        {128, 128, "128B"},
        {256, 256, "256B"},
        {512, 512, "512B"},
        {1024, 1024, "1KB"},
        {4096, 4096, "4KB"},
        {8192, 8192, "8KB"},
        {65536, 65536, "64KB"},
        {262144, 262144, "256KB"},
    };

    for (auto& fs : fixed_sizes) {
        TestResult tc = run_bench_tc(1, 100000, 3, fs);
        if (tc.success) {
            double alloc_ns_per = tc.alloc_time_us * 1000.0 / tc.total_ops;
            double free_ns_per = tc.free_time_us * 1000.0 / tc.total_ops;
            double total_ns_per = tc.total_time_us * 1000.0 / tc.total_ops;
            printf("%-10s %-14s %10.1f %10.1f %10.1f\n",
                   fs.name, "TCMalloc", alloc_ns_per, free_ns_per, total_ns_per);
        }

        TestResult sys = run_bench_sys(1, 100000, 3, fs);
        if (sys.success) {
            double alloc_ns_per = sys.alloc_time_us * 1000.0 / sys.total_ops;
            double free_ns_per = sys.free_time_us * 1000.0 / sys.total_ops;
            double total_ns_per = sys.total_time_us * 1000.0 / sys.total_ops;
            printf("%-10s %-14s %10.1f %10.1f %10.1f\n",
                   fs.name, "SystemMalloc", alloc_ns_per, free_ns_per, total_ns_per);
        }

        if (tc.success && sys.success) {
            double sp = sys.total_time_us / tc.total_time_us;
            printf("           => %.2fx (%s)\n\n", sp, sp > 1.0 ? "TC FASTER" : "SM FASTER");
        }
    }

    printf("================================================================\n");
    printf("  Test 05 Completed\n");
    printf("================================================================\n");

    return 0;
}
