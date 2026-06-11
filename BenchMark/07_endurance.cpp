#include "common.h"

// Test 07: Endurance / Long-Running Stability
// 12线程持续运行60秒，每10秒输出一次吞吐量和RSS

int main() {
    printf("================================================================\n");
    printf("  Test 07: Endurance Test (12 threads, 60 seconds)\n");
    printf("================================================================\n\n");

    print_system_info();
    warmup();

    const int THREADS = 12;
    const int DURATION_SEC = 60;
    const int REPORT_INTERVAL = 10;

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> total_ops{0};
    std::atomic<bool> failed{false};

    auto worker = [&]() {
        std::vector<void*> v;
        v.reserve(256);
        while (!stop.load(std::memory_order_relaxed)) {
            v.clear();
            for (int i = 0; i < 256; i++) {
                size_t sz = pick_size(SZ_MIXED, i);
                void* p = TCMemAllocator::Alloc(sz);
                if (!p) { failed = true; return; }
                v.push_back(p);
            }
            for (auto p : v) TCMemAllocator::Free(p);
            total_ops.fetch_add(256, std::memory_order_relaxed);
        }
    };

    printf("%-12s %12s %12s %8s\n", "Time(s)", "Total Ops", "Kops/s", "RSS(MB)");
    printf("%s\n", std::string(50, '-').c_str());

    auto start_time = Clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; t++) {
        threads.emplace_back(worker);
    }

    for (int sec = REPORT_INTERVAL; sec <= DURATION_SEC; sec += REPORT_INTERVAL) {
        std::this_thread::sleep_for(std::chrono::seconds(REPORT_INTERVAL));
        uint64_t ops = total_ops.load(std::memory_order_relaxed);
        auto now = Clock::now();
        double elapsed_ms = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count() / 1000.0;
        double kops = (double)ops / (elapsed_ms / 1000.0) / 1000.0;
        size_t rss = get_rss_mb();
        printf("%-12d %12zu %12.1f %8zu\n", sec, ops, kops, rss);
    }

    stop = true;
    for (auto& t : threads) t.join();

    if (failed.load()) {
        printf("\nFAILED: OOM during endurance test\n");
    } else {
        uint64_t ops = total_ops.load();
        auto end_time = Clock::now();
        double total_ms = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 1000.0;
        double avg_kops = (double)ops / (total_ms / 1000.0) / 1000.0;
        printf("\nTotal: %zu ops in %.1f seconds, avg %.1f Kops/s\n",
               ops, total_ms / 1000.0, avg_kops);
    }

    printf("\n================================================================\n");
    printf("  Test 07 Completed\n");
    printf("================================================================\n");

    return 0;
}
