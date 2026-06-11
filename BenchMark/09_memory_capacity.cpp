#include "common.h"

// Test 09: Memory Capacity Limit
// 12线程混合大小，递增操作数，监控RSS，接近物理内存80%时停止

int main() {
    printf("================================================================\n");
    printf("  Test 09: Memory Capacity Limit (12 threads)\n");
    printf("================================================================\n\n");

    print_system_info();
    warmup();

    size_t total_mem_mb = get_total_memory_mb();
    size_t limit_mb = total_mem_mb * 80 / 100; // 80% of physical memory
    printf("Physical memory: %zu MB, safety limit: %zu MB (80%%)\n\n", total_mem_mb, limit_mb);

    printf("%-12s %-14s %10s %10s %12s %8s\n",
           "Ops/Thr", "Allocator", "Alloc(ms)", "Free(ms)", "T(Kops/s)", "RSS(MB)");
    printf("%s\n", std::string(80, '-').c_str());

    size_t ops_list[] = {10000, 50000, 100000, 200000, 500000, 1000000};

    for (size_t ops : ops_list) {
        // 先检查当前RSS，如果已经接近限制则跳过
        size_t current_rss = get_rss_mb();
        if (current_rss > limit_mb) {
            printf("%-12zu SKIPPED: RSS (%zu MB) exceeds safety limit (%zu MB)\n",
                   ops, current_rss, limit_mb);
            continue;
        }

        TestResult tc = run_bench_tc(12, ops, 1, SZ_MIXED);
        size_t rss = get_rss_mb();
        if (tc.success) {
            printf("%-12zu %-14s %10.2f %10.2f %12.1f %8zu\n",
                   ops, "TCMalloc", tc.alloc_time_us/1000.0, tc.free_time_us/1000.0,
                   tc.total_throughput_kops, rss);
        } else {
            printf("%-12zu %-14s FAILED: %s\n", ops, "TCMalloc", tc.error_msg.c_str());
        }

        TestResult sys = run_bench_sys(12, ops, 1, SZ_MIXED);
        rss = get_rss_mb();
        if (sys.success) {
            printf("%-12zu %-14s %10.2f %10.2f %12.1f %8zu\n",
                   ops, "SystemMalloc", sys.alloc_time_us/1000.0, sys.free_time_us/1000.0,
                   sys.total_throughput_kops, rss);
        } else {
            printf("%-12zu %-14s FAILED: %s\n", ops, "SystemMalloc", sys.error_msg.c_str());
        }

        if (tc.success && sys.success) {
            double sp = sys.total_time_us / tc.total_time_us;
            printf("             => %.2fx (%s)\n\n", sp, sp > 1.0 ? "TC FASTER" : "SM FASTER");
        }

        // RSS超过安全阈值则停止后续测试
        if (rss > limit_mb) {
            printf("\nRSS (%zu MB) exceeded safety limit (%zu MB), stopping.\n", rss, limit_mb);
            break;
        }
    }

    printf("\n================================================================\n");
    printf("  Test 09 Completed\n");
    printf("================================================================\n");

    return 0;
}
