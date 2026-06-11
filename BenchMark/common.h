#pragma once
#include "../src/CMalloc.h"
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>
#include <algorithm>
#include <random>
#include <sys/resource.h>
#include <unistd.h>

using namespace CMP;

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::nanoseconds;

static double ns_to_us(double ns) { return ns / 1e3; }

struct TestResult {
    std::string allocator_name;
    size_t threads;
    size_t ntimes;
    size_t rounds;
    double alloc_time_us;
    double free_time_us;
    double total_time_us;
    double alloc_throughput_kops;
    double free_throughput_kops;
    double total_throughput_kops;
    size_t total_ops;
    bool success;
    std::string error_msg;
};

struct SizeDist {
    size_t min_size;
    size_t max_size;
    const char* name;
};

static const SizeDist SZ_SMALL     = {1, 128, "small(1-128B)"};
static const SizeDist SZ_MEDIUM    = {129, 1024, "medium(129-1024B)"};
static const SizeDist SZ_LARGE     = {1025, 8192, "large(1-8KB)"};
static const SizeDist SZ_HUGE      = {8193, 65536, "huge(8-64KB)"};
static const SizeDist SZ_SUPERHUGE = {65537, 262144, "superhuge(64-256KB)"};
static const SizeDist SZ_MIXED     = {1, 8192, "mixed(1-8KB)"};

static size_t pick_size(const SizeDist& d, size_t i) {
    return d.min_size + (i % (d.max_size - d.min_size + 1));
}

static size_t get_rss_mb() {
#ifdef __linux__
    FILE* f = fopen("/proc/self/status", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                fclose(f);
                size_t kb = 0;
                sscanf(line + 6, "%zu", &kb);
                return kb / 1024;
            }
        }
        fclose(f);
    }
#endif
    return 0;
}

static size_t get_total_memory_mb() {
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return (size_t)(pages * page_size) / (1024 * 1024);
}

static int get_cpu_count() {
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
}

static void drop_caches() {
    sync();
    FILE* f = fopen("/proc/sys/vm/drop_caches", "w");
    if (f) {
        fprintf(f, "3");
        fclose(f);
    }
}

static void warmup() {
    // 预热TCMalloc分配器，确保ThreadCache已初始化
    for (size_t sz : {8, 64, 256, 1024, 4096, 65536}) {
        void* p = TCMemAllocator::Alloc(sz);
        if (p) TCMemAllocator::Free(p);
    }
    // 预热系统malloc
    for (size_t sz : {8, 64, 256, 1024, 4096, 65536}) {
        void* p = malloc(sz);
        if (p) free(p);
    }
}

static void print_system_info() {
    printf("System: %d CPUs, %zu MB RAM\n", get_cpu_count(), get_total_memory_mb());
}

static const size_t BATCH_SIZE = 1024;

// ==================== TCMalloc benchmark ====================
inline TestResult run_bench_tc(size_t threads, size_t ntimes, size_t rounds,
                                const SizeDist& dist) {
    TestResult r;
    r.allocator_name = "TCMalloc";
    r.threads = threads;
    r.ntimes = ntimes;
    r.rounds = rounds;
    r.success = false;

    try {
        std::vector<std::thread> vthread(threads);
        std::atomic<uint64_t> alloc_ns{0};
        std::atomic<uint64_t> free_ns{0};
        std::atomic<bool> failed{false};

        for (size_t k = 0; k < threads; ++k) {
            vthread[k] = std::thread([&, k]() {
                std::vector<void*> v;
                v.reserve(BATCH_SIZE);

                for (size_t j = 0; j < rounds; ++j) {
                    size_t ops_done = 0;
                    while (ops_done < ntimes) {
                        size_t batch = std::min(BATCH_SIZE, ntimes - ops_done);
                        v.clear();
                        auto t1 = Clock::now();
                        for (size_t i = 0; i < batch; i++) {
                            void* ptr = TCMemAllocator::Alloc(pick_size(dist, ops_done + i));
                            if (!ptr) { failed = true; return; }
                            v.push_back(ptr);
                        }
                        auto t2 = Clock::now();
                        for (size_t i = 0; i < v.size(); i++) TCMemAllocator::Free(v[i]);
                        auto t3 = Clock::now();
                        alloc_ns.fetch_add(
                            std::chrono::duration_cast<Duration>(t2 - t1).count(),
                            std::memory_order_relaxed);
                        free_ns.fetch_add(
                            std::chrono::duration_cast<Duration>(t3 - t2).count(),
                            std::memory_order_relaxed);
                        ops_done += batch;
                    }
                }
            });
        }

        for (auto& t : vthread) t.join();

        if (failed) {
            r.error_msg = "OOM";
            return r;
        }

        double a_us = ns_to_us((double)alloc_ns.load());
        double f_us = ns_to_us((double)free_ns.load());
        double t_us = a_us + f_us;
        size_t total = threads * rounds * ntimes;

        r.alloc_time_us = a_us;
        r.free_time_us = f_us;
        r.total_time_us = t_us;
        r.total_ops = total;
        r.alloc_throughput_kops = total / (a_us / 1e6) / 1000.0;
        r.free_throughput_kops = total / (f_us / 1e6) / 1000.0;
        r.total_throughput_kops = total / (t_us / 1e6) / 1000.0;
        r.success = true;
    } catch (const std::exception& e) {
        r.error_msg = e.what();
    }
    return r;
}

// ==================== System malloc benchmark ====================
inline TestResult run_bench_sys(size_t threads, size_t ntimes, size_t rounds,
                                 const SizeDist& dist) {
    TestResult r;
    r.allocator_name = "SystemMalloc";
    r.threads = threads;
    r.ntimes = ntimes;
    r.rounds = rounds;
    r.success = false;

    try {
        std::vector<std::thread> vthread(threads);
        std::atomic<uint64_t> alloc_ns{0};
        std::atomic<uint64_t> free_ns{0};
        std::atomic<bool> failed{false};

        for (size_t k = 0; k < threads; ++k) {
            vthread[k] = std::thread([&, k]() {
                std::vector<void*> v;
                v.reserve(BATCH_SIZE);

                for (size_t j = 0; j < rounds; ++j) {
                    size_t ops_done = 0;
                    while (ops_done < ntimes) {
                        size_t batch = std::min(BATCH_SIZE, ntimes - ops_done);
                        v.clear();
                        auto t1 = Clock::now();
                        for (size_t i = 0; i < batch; i++) {
                            void* ptr = malloc(pick_size(dist, ops_done + i));
                            if (!ptr) { failed = true; return; }
                            v.push_back(ptr);
                        }
                        auto t2 = Clock::now();
                        for (size_t i = 0; i < v.size(); i++) free(v[i]);
                        auto t3 = Clock::now();
                        alloc_ns.fetch_add(
                            std::chrono::duration_cast<Duration>(t2 - t1).count(),
                            std::memory_order_relaxed);
                        free_ns.fetch_add(
                            std::chrono::duration_cast<Duration>(t3 - t2).count(),
                            std::memory_order_relaxed);
                        ops_done += batch;
                    }
                }
            });
        }

        for (auto& t : vthread) t.join();

        if (failed) {
            r.error_msg = "OOM";
            return r;
        }

        double a_us = ns_to_us((double)alloc_ns.load());
        double f_us = ns_to_us((double)free_ns.load());
        double t_us = a_us + f_us;
        size_t total = threads * rounds * ntimes;

        r.alloc_time_us = a_us;
        r.free_time_us = f_us;
        r.total_time_us = t_us;
        r.total_ops = total;
        r.alloc_throughput_kops = total / (a_us / 1e6) / 1000.0;
        r.free_throughput_kops = total / (f_us / 1e6) / 1000.0;
        r.total_throughput_kops = total / (t_us / 1e6) / 1000.0;
        r.success = true;
    } catch (const std::exception& e) {
        r.error_msg = e.what();
    }
    return r;
}

// ==================== Print helpers ====================
inline void print_header() {
    printf("| %-14s | %4s | %7s | %5s | %10s | %10s | %10s | %10s | %10s | %10s | %-16s |\n",
           "Allocator", "Thr", "Ntimes", "Rnd", "Alloc(ms)", "Free(ms)", "Total(ms)",
           "A(Kops/s)", "F(Kops/s)", "T(Kops/s)", "Size");
    printf("|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|\n",
           std::string(16,'-').c_str(), std::string(6,'-').c_str(), std::string(9,'-').c_str(),
           std::string(7,'-').c_str(), std::string(12,'-').c_str(), std::string(12,'-').c_str(),
           std::string(12,'-').c_str(), std::string(12,'-').c_str(), std::string(12,'-').c_str(),
           std::string(12,'-').c_str(), std::string(18,'-').c_str());
}

inline void print_row(const TestResult& r, const char* sname) {
    if (!r.success) {
        printf("| %-14s | %4zu | %7zu | %5zu | FAILED: %-60s |\n",
               r.allocator_name.c_str(), r.threads, r.ntimes, r.rounds, r.error_msg.c_str());
        return;
    }
    printf("| %-14s | %4zu | %7zu | %5zu | %10.2f | %10.2f | %10.2f | %10.1f | %10.1f | %10.1f | %-16s |\n",
           r.allocator_name.c_str(), r.threads, r.ntimes, r.rounds,
           r.alloc_time_us / 1000.0, r.free_time_us / 1000.0, r.total_time_us / 1000.0,
           r.alloc_throughput_kops, r.free_throughput_kops, r.total_throughput_kops, sname);
}

inline void print_cmp(const TestResult& tc, const TestResult& sys) {
    if (!tc.success || !sys.success) {
        printf("  [VS] Cannot compare\n");
        return;
    }
    double sa = sys.alloc_time_us / tc.alloc_time_us;
    double sf = sys.free_time_us / tc.free_time_us;
    double st = sys.total_time_us / tc.total_time_us;
    printf("  [VS] TC/SM: Alloc %.2fx, Free %.2fx, Total %.2fx => %s\n",
           sa, sf, st, st > 1.0 ? "TC FASTER" : "SM FASTER");
}
