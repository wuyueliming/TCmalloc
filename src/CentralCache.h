#pragma once
#include"common.h"
namespace CMP
{
    class CentralCache {
        public:
        CentralCache() {
        }
        CentralCache(const CentralCache&) = delete;
        static CentralCache* GetInstance() {
            return &_Instance;
        }
        public:
        //分配内存给ThreadCache
        size_t Allocate(void*& start, void*& end, size_t need, size_t size);
        //回收ThreadCache的内存
        void Deallocate(void* freelist, size_t size, size_t cnt);
        private:
        Span* getOneSpan(size_t index,size_t size);
        size_t FetchToThreadCache(void*& start,void*& end,size_t need,size_t size);
        void RecycleListToSpans(void* freelist, size_t size,size_t cnt);
        private:
        SpanList _SpanLists[NLISTS];
        static CentralCache _Instance;
    };
};
