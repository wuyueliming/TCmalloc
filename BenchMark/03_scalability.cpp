#include "common.h"

// Test 03: Multi-Thread Scalability
// 测试 TCMalloc 在不同线程数下的扩展性（1/2/4/8/12线程）

int main() {
    printf("================================================================\n");
    printf("  Test 03: Multi-Thread Scalability\n");
    printf("================================================================\n\n");

    print_system_info();
    warmup();

    struct Config {
        SizeDist dist;
        size_t ntimes;
        size_t rounds;
    };

    Config configs[] = {
        {SZ_SMALL,  100000, 3},
        {SZ_MEDIUM, 100000, 3},
        {SZ_MIXED,  100000, 3},
    };

    size_t thread_counts[] = {1, 2, 4, 8, 12};

    for (auto& cfg : configs) {
        printf("--- Size: %s ---\n\n", cfg.dist.name);
        print_header();

        for (size_t t : thread_counts) {
            TestResult tc = run_bench_tc(t, cfg.ntimes, cfg.rounds, cfg.dist);
            print_row(tc, cfg.dist.name);

            TestResult sys = run_bench_sys(t, cfg.ntimes, cfg.rounds, cfg.dist);
            print_row(sys, cfg.dist.name);

            print_cmp(tc, sys);
        }
        printf("\n");
    }

    // 扩展性汇总
    printf("\n--- Scalability Summary (mixed size, TCMalloc) ---\n\n");
    printf("%-8s %12s %12s %12s\n", "Threads", "Total(ms)", "T(Kops/s)", "Scale");
    printf("%s\n", std::string(50, '-').c_str());

    double base_throughput = 0;
    for (size_t t : thread_counts) {
        TestResult tc = run_bench_tc(t, 100000, 3, SZ_MIXED);
        if (tc.success) {
            double scale = (base_throughput > 0) ? tc.total_throughput_kops / base_throughput : 1.0;
            if (base_throughput == 0) base_throughput = tc.total_throughput_kops;
            printf("%-8zu %12.2f %12.1f %12.2fx\n",
                   t, tc.total_time_us / 1000.0, tc.total_throughput_kops, scale);
        }
    }

    printf("\n================================================================\n");
    printf("  Test 03 Completed\n");
    printf("================================================================\n");

    return 0;
}
