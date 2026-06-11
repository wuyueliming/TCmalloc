#include "common.h"

// Test 02: Single-Thread Performance Comparison
// 单线程下 TCMalloc vs SystemMalloc 的性能对比

int main() {
    printf("================================================================\n");
    printf("  Test 02: Single-Thread Performance Comparison\n");
    printf("================================================================\n\n");

    print_system_info();
    warmup();

    struct Config {
        SizeDist dist;
        size_t ntimes;
        size_t rounds;
    };

    Config configs[] = {
        {SZ_SMALL,     100000, 3},
        {SZ_MEDIUM,    100000, 3},
        {SZ_LARGE,     100000, 3},
        {SZ_HUGE,       50000, 3},
        {SZ_SUPERHUGE,  50000, 3},
    };

    print_header();

    for (auto& cfg : configs) {
        TestResult tc = run_bench_tc(1, cfg.ntimes, cfg.rounds, cfg.dist);
        print_row(tc, cfg.dist.name);

        TestResult sys = run_bench_sys(1, cfg.ntimes, cfg.rounds, cfg.dist);
        print_row(sys, cfg.dist.name);

        print_cmp(tc, sys);
        printf("\n");
    }

    printf("================================================================\n");
    printf("  Test 02 Completed\n");
    printf("================================================================\n");

    return 0;
}
