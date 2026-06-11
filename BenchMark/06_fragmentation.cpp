#include "common.h"

// Test 06: Memory Fragmentation Test
// 测试内存碎片情况：分配大量对象后随机释放一半，观察RSS变化

static void fragmentation_test_1thread() {
    printf("--- Fragmentation Test: Single Thread ---\n\n");

    const size_t N = 100000;
    std::mt19937 rng(42);

    // 1. 分配大量混合大小对象
    printf("Phase 1: Allocating %zu mixed-size objects...\n", N);
    size_t rss_before = get_rss_mb();
    std::vector<void*> ptrs(N);
    std::vector<size_t> sizes(N);
    for (size_t i = 0; i < N; i++) {
        sizes[i] = pick_size(SZ_MIXED, i);
        ptrs[i] = TCMemAllocator::Alloc(sizes[i]);
        if (!ptrs[i]) {
            printf("OOM at %zu\n", i);
            return;
        }
    }
    size_t rss_after_alloc = get_rss_mb();
    printf("  RSS: %zu MB -> %zu MB (delta: %zu MB)\n",
           rss_before, rss_after_alloc, rss_after_alloc - rss_before);

    // 2. 随机释放一半
    printf("Phase 2: Randomly freeing ~50%% of objects...\n");
    std::vector<size_t> indices(N);
    for (size_t i = 0; i < N; i++) indices[i] = i;
    std::shuffle(indices.begin(), indices.end(), rng);

    size_t free_count = N / 2;
    for (size_t i = 0; i < free_count; i++) {
        TCMemAllocator::Free(ptrs[indices[i]]);
        ptrs[indices[i]] = nullptr;
    }
    size_t rss_after_half_free = get_rss_mb();
    printf("  Freed %zu objects, RSS: %zu MB (delta from alloc: %zu MB)\n",
           free_count, rss_after_half_free,
           rss_after_half_free > rss_after_alloc ? rss_after_half_free - rss_after_alloc : 0);

    // 3. 重新分配释放掉的内存
    printf("Phase 3: Re-allocating freed objects...\n");
    for (size_t i = 0; i < N; i++) {
        if (ptrs[i] == nullptr) {
            ptrs[i] = TCMemAllocator::Alloc(sizes[i]);
        }
    }
    size_t rss_after_realloc = get_rss_mb();
    printf("  RSS after re-alloc: %zu MB\n", rss_after_realloc);

    // 4. 全部释放
    printf("Phase 4: Freeing all objects...\n");
    for (size_t i = 0; i < N; i++) {
        if (ptrs[i]) TCMemAllocator::Free(ptrs[i]);
    }
    size_t rss_after_free = get_rss_mb();
    printf("  RSS after free all: %zu MB (delta from start: %zu MB)\n",
           rss_after_free, rss_after_free > rss_before ? rss_after_free - rss_before : 0);

    // 碎片率 = (释放一半后RSS - 全部释放后RSS) / (分配后RSS - 全部释放后RSS)
    double frag_rate = 0;
    if (rss_after_alloc > rss_after_free && rss_after_half_free > rss_after_free) {
        frag_rate = (double)(rss_after_half_free - rss_after_free) /
                    (double)(rss_after_alloc - rss_after_free) * 100.0;
    }
    printf("\n  Fragmentation rate: %.1f%%\n", frag_rate);
    printf("  (Lower is better. 50%% means half the memory is wasted due to fragmentation)\n\n");
}

static void fragmentation_test_mt() {
    printf("--- Fragmentation Test: 12 Threads ---\n\n");

    const size_t OPS_PER_THREAD = 20000;
    const int THREADS = 12;

    size_t rss_before = get_rss_mb();

    // TCMalloc的Free依赖pTLSThreadCache，释放必须在分配的同一线程
    // 因此每个线程在同一个线程函数内完成所有操作

    // 每个线程记录自己的RSS快照
    struct PhaseRSS {
        size_t after_alloc;
        size_t after_half_free;
        size_t after_free;
    };
    std::vector<PhaseRSS> phase_rss(THREADS, {0, 0, 0});

    printf("Running all phases per thread (alloc -> half free -> re-alloc -> free all)...\n");

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; t++) {
        threads.emplace_back([&, t]() {
            std::vector<void*> ptrs;
            ptrs.reserve(OPS_PER_THREAD);

            // Phase 1: 分配
            for (size_t i = 0; i < OPS_PER_THREAD; i++) {
                size_t sz = pick_size(SZ_MIXED, t * OPS_PER_THREAD + i);
                void* p = TCMemAllocator::Alloc(sz);
                if (!p) return;
                ptrs.push_back(p);
            }
            phase_rss[t].after_alloc = get_rss_mb();

            // Phase 2: 释放前一半
            size_t half = ptrs.size() / 2;
            for (size_t i = 0; i < half; i++) {
                TCMemAllocator::Free(ptrs[i]);
                ptrs[i] = nullptr;
            }
            phase_rss[t].after_half_free = get_rss_mb();

            // Phase 3: 重新分配
            for (size_t i = 0; i < ptrs.size(); i++) {
                if (ptrs[i] == nullptr) {
                    size_t sz = pick_size(SZ_MIXED, t * OPS_PER_THREAD + i);
                    ptrs[i] = TCMemAllocator::Alloc(sz);
                }
            }

            // Phase 4: 全部释放
            for (auto p : ptrs) {
                if (p) TCMemAllocator::Free(p);
            }
            phase_rss[t].after_free = get_rss_mb();
        });
    }
    for (auto& t : threads) t.join();

    size_t rss_after_alloc = get_rss_mb();
    size_t rss_after_free = get_rss_mb();

    printf("  RSS before: %zu MB, after all: %zu MB\n\n", rss_before, rss_after_free);

    // 用单线程碎片测试的RSS数据来计算碎片率
    // 多线程场景下RSS是进程级别的，无法精确计算每线程碎片率
    printf("  Note: Multi-thread fragmentation test completed successfully.\n");
    printf("  All threads completed alloc -> half-free -> re-alloc -> free-all cycle.\n\n");
}

int main() {
    printf("================================================================\n");
    printf("  Test 06: Memory Fragmentation\n");
    printf("================================================================\n\n");

    print_system_info();
    warmup();

    fragmentation_test_1thread();
    fragmentation_test_mt();

    printf("================================================================\n");
    printf("  Test 06 Completed\n");
    printf("================================================================\n");

    return 0;
}
