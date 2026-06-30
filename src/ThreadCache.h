#pragma once
#include"common.h"
namespace CMP
{
    class alignas(64) ThreadCache {  // 对齐对象起始到 cache line，减少 false sharing
        public:
        ThreadCache() = default;
        //ThreadCache分配内存
        void* Allocate(size_t size);
        //ThreadCache回收内存
        void Deallocate(void* ptr, size_t size);
        private:
        void* FetchFromCentralCache(size_t size, size_t index);
        private:
        FreeList _FreeLists[NLISTS];
    };
    // TLS: thread local storage
    //static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
    static thread_local ThreadCache* pTLSThreadCache = nullptr;
};
