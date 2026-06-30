#pragma once
#include"common.h"
#include"base/ObjectPool.h"
#include"base/PageMap.h"
namespace CMP
{
    class PageCache {
        public:
        static PageCache _instance;
        static PageCache* GetInstance() {
            return &_instance;
        }
        public:
        //分配Span
        Span* Allocate(size_t kpage);
        //回收Span
        void Deallocate(Span* pSpan);
        //工具函数
        Span* IdmapToSpan(PAGE_ID id);//使用基数树实现映射，ensure会改变树结构，set修改映射，get是线程安全的。
        inline Lock& GetMutex() {
            return _mutex;
        }
        private:
        PageCache(){}
        PageCache(const PageCache&) = delete;
        Span* NewSpan(size_t kpage);
        Span* SplitKSpan(size_t i, size_t kpage);
        void MergeFreeSpan(Span* pSpan);
        Span* BuildIdMapAll(Span* pSpan);
        void BuildIdMapBoundary(Span* pSpan);
        void EraseIdMapAll(Span* pSpan);
        void EraseIdMapBoundary(Span* pSpan);
        private:
        SpanList _SpanList[NPAGES];
        Lock _mutex;
        #if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
        TCMalloc_PageMap3<64 - PAGE_SHIFT> _IdSpanMap;
        #else
        TCMalloc_PageMap3<32 - PAGE_SHIFT> _IdSpanMap;
        #endif
        ObjectPool<Span> _SpanPool;
    };
};
