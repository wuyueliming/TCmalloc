#include"CentralCache.h"
#include"PageCache.h"
namespace CMP
{
    size_t CentralCache::Allocate(void*& start, void*& end, size_t need, size_t size) {
        return FetchToThreadCache(start, end, need, size);
    }
    size_t CentralCache::FetchToThreadCache(void*& start, void*& end, size_t need, size_t size) {
        size_t index = SizeClass::Index(size);
        _SpanLists[index].GetMutex().lock();
        Span* pSpan = getOneSpan(index, size);
        assert(pSpan != nullptr && pSpan->_freeList != nullptr);
        //移出need个对象到thread cache的freelist
        size_t actual_num = 1;
        start = pSpan->_freeList;
        end = pSpan->_freeList;
        for (int i = 0; i < need - 1 && nextptr(end) != nullptr; i++) {
            end = nextptr(end);
            actual_num++;
        }
        pSpan->_freeList = nextptr(end);
        pSpan->_useCount += actual_num;
        _SpanLists[index].GetMutex().unlock();
        return actual_num;
    }
    Span* CentralCache::getOneSpan(size_t index,size_t size) {
        Span* pSpan = _SpanLists[index].Begin();
        while (pSpan != _SpanLists[index].End()) {
            if (pSpan->_freeList != nullptr) {
                return pSpan;
            }
            pSpan = pSpan->_next;
        }
        // 没有找到合适的span，向PageCache申请一个新的span
        _SpanLists[index].GetMutex().unlock();
        Span* newSpan = PageCache::GetInstance()->Allocate(SizeClass::FetchNumber_page(size));
        newSpan->_objSize = size;
        newSpan->_useCount = 0;
        //切分freelist
        char* start = (char*)(newSpan->_pageId << PAGE_SHIFT);
        char* end = start + (newSpan->_n << PAGE_SHIFT);
        while (start < end) {
            //头插
            if (start + size <= end)
            {
                nextptr(start) = newSpan->_freeList;
                newSpan->_freeList = start;
            }
            start = start + size;
        }
        _SpanLists[index].GetMutex().lock();
        _SpanLists[index].pushBack(newSpan);
        return getOneSpan(index, size);
    }
    void CentralCache::Deallocate(void* freelist, size_t size, size_t cnt) {
        RecycleListToSpans(freelist, size, cnt);
    }
    void CentralCache::RecycleListToSpans(void* freelist,size_t size,size_t num) {
        size_t index = SizeClass::Index(size);
        Span* spans_to_delete[64];  // 收集需要归还 PageCache 的 Span
        size_t delete_count = 0;

        _SpanLists[index].GetMutex().lock();
        void* next=nullptr;
        for (void* obj = freelist; obj != nullptr; obj = next) {
            next = nextptr(obj);
            //找到obj所在的span
            PAGE_ID id = (PAGE_ID)obj >> PAGE_SHIFT;
            Span* pSpan = PageCache::GetInstance()->IdmapToSpan(id);
            assert(pSpan != nullptr);
            //将obj插回span的freelist
            nextptr(obj) = pSpan->_freeList;
            pSpan->_freeList = obj;
            pSpan->_useCount--;
            //当前span的内存全部回收了，将span收集起来，稍后在锁外归还PageCache
            if (pSpan->_useCount == 0) {
                _SpanLists[index].Erase(pSpan);
                if (delete_count < 64) {
                    spans_to_delete[delete_count++] = pSpan;
                }
            }
        }
        _SpanLists[index].GetMutex().unlock();

        // 在 CentralCache 锁外释放 Span，避免锁嵌套
        for (size_t i = 0; i < delete_count; i++) {
            PageCache::GetInstance()->Deallocate(spans_to_delete[i]);
        }
    }
    CentralCache CentralCache::_Instance;
};
