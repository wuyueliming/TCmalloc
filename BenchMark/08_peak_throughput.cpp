#include "common.h"

// Test 08: Peak Throughput
// 12线程小对象，递增操作数，测量峰值吞吐量

int main() {
    printf("================================================================\n");
    printf("  Test 08: Peak Throughput (12 threads, small objects)\n");
    printf("================================================================\n\n");

    print_system_info();
    warmup();

    printf("%-12s %-14s %10s %12s %8s\n",
           "Ops/Thr", "Allocator", "Total(ms)", "T(Kops/s)", "RSS(MB)");
    printf("%s\n", std::string(65, '-').c_str());

    size_t ops_list[] = {100000, 500000, 1000000, 2000000, 5000000};

    for (size_t ops : ops_list) {
        TestResult tc = run_bench_tc(12, ops, 1, SZ_SMALL);
        size_t rss = get_rss_mb();
        if (tc.success) {
            printf("%-12zu %-14s %10.2f %12.1f %8zu\n",
                   ops, "TCMalloc", tc.total_time_us/1000.0, tc.total_throughput_kops, rss);
        } else {
            printf("%-12zu %-14s FAILED: %s\n", ops, "TCMalloc", tc.error_msg.c_str());
        }

        TestResult sys = run_bench_sys(12, ops, 1, SZ_SMALL);
        rss = get_rss_mb();
        if (sys.success) {
            printf("%-12zu %-14s %10.2f %12.1f %8zu\n",
                   ops, "SystemMalloc", sys.total_time_us/1000.0, sys.total_throughput_kops, rss);
        } else {
            printf("%-12zu %-14s FAILED: %s\n", ops, "SystemMalloc", sys.error_msg.c_str());
        }

        if (tc.success && sys.success) {
            double sp = sys.total_time_us / tc.total_time_us;
            printf("             => %.2fx (%s)\n\n", sp, sp > 1.0 ? "TC FASTER" : "SM FASTER");
        }
    }

    printf("================================================================\n");
    printf("  Test 08 Completed\n");
    printf("================================================================\n");

    return 0;
}
