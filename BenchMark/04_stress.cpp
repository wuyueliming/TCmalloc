#include "common.h"

// Test 04: Stress Test
// 12线程高负载压力测试

int main() {
    printf("================================================================\n");
    printf("  Test 04: Stress Test (12 threads)\n");
    printf("================================================================\n\n");

    print_system_info();
    warmup();

    struct Config {
        SizeDist dist;
        size_t ntimes;
        size_t rounds;
        const char* label;
    };

    Config configs[] = {
        {SZ_SMALL,     200000, 3, "12T-200K-small"},
        {SZ_MEDIUM,    200000, 3, "12T-200K-medium"},
        {SZ_MIXED,     200000, 3, "12T-200K-mixed"},
        {SZ_LARGE,     100000, 3, "12T-100K-large"},
        {SZ_HUGE,       50000, 3, "12T-50K-huge"},
        {SZ_SUPERHUGE,  50000, 3, "12T-50K-superhuge"},
    };

    print_header();

    for (auto& cfg : configs) {
        TestResult tc = run_bench_tc(12, cfg.ntimes, cfg.rounds, cfg.dist);
        print_row(tc, cfg.label);

        TestResult sys = run_bench_sys(12, cfg.ntimes, cfg.rounds, cfg.dist);
        print_row(sys, cfg.label);

        print_cmp(tc, sys);
        printf("\n");
    }

    printf("================================================================\n");
    printf("  Test 04 Completed\n");
    printf("================================================================\n");

    return 0;
}
