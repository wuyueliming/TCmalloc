#pragma once
#include"base/ObjectPool.h"
#include"PageCache.h"
#include"common.h"
#include"ThreadCache.h"
#include"CentralCache.h"
#include"PageCache.h"
namespace CMP
{
    class TCMemAllocator {
        public:
        //是否要align内存的起始地址？？？
        static void* Alloc(size_t size) {
            //小于256KB的请求通过三层内存池
            if (size <= MAX_BYTES)
            {
                // 通过TLS,每个线程无锁的获取自己的专属的ThreadCache对象
                if (pTLSThreadCache == nullptr)
                {
                    static ObjectPool<ThreadCache> TCpool;
                    pTLSThreadCache = TCpool.New();
                    //pTLSThreadCache = new ThreadCache;
                }
                //cout << std::this_thread::get_id() << ":" << pTLSThreadCache << endl;
                return pTLSThreadCache->Allocate(size);
            }//大于256KB的请求直接通过PageCache分配
            else {
                size_t kpage = SizeClass::RoundUp(size) >> PAGE_SHIFT;
                Span* pSpan = PageCache::GetInstance()->Allocate(kpage);
                pSpan->_objSize = size;
                return (void*)(pSpan->_pageId << PAGE_SHIFT);
            }
        }
        static void Free(void* ptr) {
            Span* retSpan = PageCache::GetInstance()->IdmapToSpan((PAGE_ID)ptr >> PAGE_SHIFT);
            if (retSpan == nullptr)
            return;
            size_t size = retSpan->_objSize;
            if (size == 0) return;
            if (size <= MAX_BYTES) {
                //ptr只能从分配内存的起始地址开始释放，防止freelist中插入错误的地址再次分配时出问题
                size_t offset = (char*)ptr - (char*)(retSpan->_pageId << PAGE_SHIFT);
                assert(offset % size == 0);
                pTLSThreadCache->Deallocate(ptr, size);
            }
            else {
                PageCache::GetInstance()->Deallocate(retSpan);
            }
        }
};
}
