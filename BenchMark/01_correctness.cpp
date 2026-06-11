#include "common.h"

// Test 01: Correctness Verification
// 验证TCMalloc分配器的基本功能正确性

bool verify_correctness() {
    printf("=== Correctness Verification ===\n");
    bool pass = true;
    auto test = [&](const char* name, bool ok) {
        printf("[Test] %-55s %s\n", name, ok ? "PASS" : "FAIL");
        if (!ok) pass = false;
    };

    // 1. 小对象分配
    {
        void* p = TCMemAllocator::Alloc(8);
        test("Small alloc (8B)", p != nullptr);
        if (p) TCMemAllocator::Free(p);
    }
    // 2. 中等对象分配
    {
        void* p = TCMemAllocator::Alloc(1024);
        test("Medium alloc (1KB)", p != nullptr);
        if (p) TCMemAllocator::Free(p);
    }
    // 3. 大对象分配
    {
        void* p = TCMemAllocator::Alloc(64 * 1024);
        test("Large alloc (64KB)", p != nullptr);
        if (p) TCMemAllocator::Free(p);
    }
    // 4. 超大对象分配
    {
        void* p = TCMemAllocator::Alloc(256 * 1024);
        test("Superhuge alloc (256KB)", p != nullptr);
        if (p) TCMemAllocator::Free(p);
    }
    // 5. 超限对象分配（>256KB，走PageCache直接分配）
    {
        void* p = TCMemAllocator::Alloc(512 * 1024);
        test("Over-limit alloc (512KB, PageCache path)", p != nullptr);
        if (p) TCMemAllocator::Free(p);
    }
    // 6. 更大对象（1MB）
    {
        void* p = TCMemAllocator::Alloc(1024 * 1024);
        test("Large PageCache alloc (1MB)", p != nullptr);
        if (p) TCMemAllocator::Free(p);
    }
    // 7. 批量分配释放
    {
        bool ok = true;
        std::vector<void*> ptrs;
        for (int i = 0; i < 1000; i++) {
            void* p = TCMemAllocator::Alloc(64);
            if (!p) { ok = false; break; }
            ptrs.push_back(p);
        }
        for (auto p : ptrs) TCMemAllocator::Free(p);
        test("Batch alloc/free (1000 x 64B)", ok);
    }
    // 8. 多线程并发（12线程）
    {
        std::atomic<bool> ok{true};
        std::vector<std::thread> threads;
        for (int t = 0; t < 12; t++) {
            threads.emplace_back([&ok]() {
                std::vector<void*> ptrs;
                for (int i = 0; i < 500; i++) {
                    void* p = TCMemAllocator::Alloc(128);
                    if (!p) { ok = false; return; }
                    ptrs.push_back(p);
                }
                for (auto p : ptrs) TCMemAllocator::Free(p);
            });
        }
        for (auto& t : threads) t.join();
        test("Multi-thread (12T x 500 x 128B)", ok.load());
    }
    // 9. 内存写入验证
    {
        bool ok = true;
        for (size_t sz : {8, 64, 256, 1024, 4096, 65536, 262144}) {
            void* p = TCMemAllocator::Alloc(sz);
            if (!p) { ok = false; break; }
            memset(p, 0xAB, sz);
            unsigned char* buf = (unsigned char*)p;
            for (size_t i = 0; i < sz; i++) {
                if (buf[i] != 0xAB) { ok = false; break; }
            }
            TCMemAllocator::Free(p);
            if (!ok) break;
        }
        test("Write & verify memory content", ok);
    }
    // 10. 重复分配释放
    {
        bool ok = true;
        for (int i = 0; i < 10000; i++) {
            void* p = TCMemAllocator::Alloc(32);
            if (!p) { ok = false; break; }
            TCMemAllocator::Free(p);
        }
        test("Repeated alloc/free (10000 x 32B)", ok);
    }
    // 11. 边界大小
    {
        bool ok = true;
        for (size_t sz : {1, 2, 4, 7, 15, 31, 63, 127, 128, 129, 255, 512, 1023, 1024,
                          1025, 2048, 4096, 8191, 16384, 32768, 65536, 131072, 262144}) {
            void* p = TCMemAllocator::Alloc(sz);
            if (!p) { ok = false; break; }
            TCMemAllocator::Free(p);
        }
        test("Boundary size alloc/free", ok);
    }
    // 12. 交叉分配释放
    {
        bool ok = true;
        void* a = TCMemAllocator::Alloc(64);
        void* b = TCMemAllocator::Alloc(128);
        if (!a || !b) ok = false;
        if (a) TCMemAllocator::Free(a);
        void* c = TCMemAllocator::Alloc(256);
        if (!c) ok = false;
        if (b) TCMemAllocator::Free(b);
        if (c) TCMemAllocator::Free(c);
        test("Interleaved alloc/free (A,B,freeA,C,freeB,freeC)", ok);
    }
    // 13. 大对象PageCache路径验证（>256KB，分配后写入再释放）
    {
        bool ok = true;
        size_t big_sizes[] = {256 * 1024 + 1, 512 * 1024, 1024 * 1024};
        for (size_t sz : big_sizes) {
            void* p = TCMemAllocator::Alloc(sz);
            if (!p) { ok = false; break; }
            memset(p, 0xCD, sz);
            unsigned char* buf = (unsigned char*)p;
            if (buf[0] != 0xCD || buf[sz - 1] != 0xCD) { ok = false; }
            TCMemAllocator::Free(p);
            if (!ok) break;
        }
        test("Large PageCache write & verify (>256KB)", ok);
    }
    // 14. 多线程混合大小
    {
        std::atomic<bool> ok{true};
        std::vector<std::thread> threads;
        for (int t = 0; t < 12; t++) {
            threads.emplace_back([&ok, t]() {
                std::vector<void*> ptrs;
                size_t sizes[] = {8, 64, 256, 1024, 4096, 65536};
                for (int i = 0; i < 200; i++) {
                    size_t sz = sizes[(t + i) % 6];
                    void* p = TCMemAllocator::Alloc(sz);
                    if (!p) { ok = false; return; }
                    ptrs.push_back(p);
                }
                for (auto p : ptrs) TCMemAllocator::Free(p);
            });
        }
        for (auto& t : threads) t.join();
        test("Multi-thread mixed size (12T x 200, various sizes)", ok.load());
    }

    printf("\nCorrectness: %s\n\n", pass ? "ALL PASSED" : "SOME FAILED");
    return pass;
}

int main() {
    printf("================================================================\n");
    printf("  Test 01: Correctness Verification\n");
    printf("================================================================\n\n");

    print_system_info();
    warmup();

    bool pass = verify_correctness();

    printf("================================================================\n");
    printf("  Result: %s\n", pass ? "ALL PASSED" : "SOME FAILED");
    printf("================================================================\n");

    return pass ? 0 : 1;
}
