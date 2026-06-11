#include"PageCache.h"
namespace CMP
{
    Span* PageCache::Allocate(size_t kpage) {
        if (kpage >= NPAGES) {
            Span* newspan = _SpanPool.New();
            void* ptr = SystemAlloc(kpage);
            newspan->_n = kpage;
            newspan->_pageId = (PAGE_ID)ptr >> PAGE_SHIFT;
            newspan->_isUse = true;
            newspan->_objSize = 0;
            newspan->_useCount = 0;
            newspan->_freeList = nullptr;
            _mutex.lock();
            BuildIdMapAll(newspan);
            _mutex.unlock();
            return newspan;
        }
        else {
            _mutex.lock();
            Span* ret = NewSpan(kpage);
            ret->_isUse = true;
            ret->_objSize = 0;
            ret->_useCount = 0;
            ret->_freeList = nullptr;
            _mutex.unlock();
            return ret;
        }
    }
    Span* PageCache::NewSpan(size_t kpage) {
        //检查是否有空闲的kpage span
        if (!_SpanList[kpage].empty()) {
            return BuildIdMapAll(_SpanList[kpage].popFront());
        }
        //没有空闲span，找到更大页数的span进行切分
        for (size_t i = kpage + 1; i < NPAGES; i++) {
            if (_SpanList[i].empty()) {
                continue;
            }
            //找到大span，进行切分
            return SplitKSpan(i, kpage);
        }
        //没有空闲大span，申请一个128页的大span
        //Span* newspan = new Span;
        Span* newspan = _SpanPool.New();
        newspan->_n = NPAGES - 1;
        newspan->_pageId = (PAGE_ID)SystemAlloc(NPAGES - 1) >> PAGE_SHIFT;
        _SpanList[NPAGES - 1].pushFront(newspan);
        BuildIdMapBoundary(newspan);
        return SplitKSpan(NPAGES - 1, kpage);
    }
    Span* PageCache::SplitKSpan(size_t i, size_t kpage) {
        Span* bigspan = _SpanList[i].popFront();
        Span* kSpan = _SpanPool.New();
        kSpan->_pageId = bigspan->_pageId;
        kSpan->_n = kpage;
        bigspan->_pageId += kpage;
        bigspan->_n -= kpage;
        if (bigspan->_n > 0) {
            _SpanList[bigspan->_n].pushFront(bigspan);
            BuildIdMapBoundary(bigspan);
        }
        else {
            _SpanPool.Delete(bigspan);
        }
        return BuildIdMapAll(kSpan);
    }
    void PageCache::Deallocate(Span* pSpan) {
        if (pSpan->_n >= NPAGES) {
            _mutex.lock();
            EraseIdMapAll(pSpan);
            pSpan->_isUse = false;
            _mutex.unlock();
            SystemFree((void*)(pSpan->_pageId << PAGE_SHIFT), pSpan->_n << PAGE_SHIFT);
            _SpanPool.Delete(pSpan);
        }
        else {
            _mutex.lock();
            EraseIdMapAll(pSpan);
            pSpan->_isUse = false;
            MergeFreeSpan(pSpan);
            _mutex.unlock();
        }
    }
    void PageCache::MergeFreeSpan(Span* pSpan) {
        //向前合并
        while (1) {
            Span* prev_span = IdmapToSpan(pSpan->_pageId - 1);
            if (prev_span == nullptr || prev_span->_isUse) {
                break;
            }
            if (pSpan->_n + prev_span->_n > NPAGES - 1) {
                break;
            }
            EraseIdMapBoundary(prev_span);
            _SpanList[prev_span->_n].Erase(prev_span);
            pSpan->_pageId = prev_span->_pageId;
            pSpan->_n += prev_span->_n;
            _SpanPool.Delete(prev_span);
        }
        //向后合并
        while (1) {
            Span* next_span = IdmapToSpan(pSpan->_pageId + pSpan->_n);
            if (next_span == nullptr || next_span->_isUse) {
                break;
            }
            if (pSpan->_n + next_span->_n > NPAGES - 1) {
                break;
            }
            EraseIdMapBoundary(next_span);
            _SpanList[next_span->_n].Erase(next_span);
            pSpan->_n += next_span->_n;
            _SpanPool.Delete(next_span);
        }
        BuildIdMapBoundary(pSpan);
        pSpan->_isUse = false;
        pSpan->_freeList = nullptr;
        _SpanList[pSpan->_n].pushFront(pSpan);
    }
    Span* PageCache::IdmapToSpan(PAGE_ID id) {
        /*auto ret = _IdSpanMap.find(id);
        if (ret == _IdSpanMap.end()) {
            return nullptr;
        }
        else {
            return ret->second;
        }*/
        return (Span*)_IdSpanMap.get(id);
    }
    Span* PageCache::BuildIdMapAll(Span* pSpan) {
        _IdSpanMap.Ensure(pSpan->_pageId, pSpan->_n);
        for (size_t i = 0; i < pSpan->_n; i++) {
            _IdSpanMap.set(pSpan->_pageId + i, pSpan);
        }
        return pSpan;
    }
    void PageCache::BuildIdMapBoundary(Span* pSpan) {
        if (pSpan->_n == 0) return;
        _IdSpanMap.Ensure(pSpan->_pageId, pSpan->_n);
        _IdSpanMap.set(pSpan->_pageId, pSpan);
        if (pSpan->_n > 1) {
            _IdSpanMap.set(pSpan->_pageId + pSpan->_n - 1, pSpan);
        }
    }
    void PageCache::EraseIdMapAll(Span* pSpan) {
        for (size_t i = 0; i < pSpan->_n; i++) {
            _IdSpanMap.set(pSpan->_pageId + i, nullptr);
        }
    }
    void PageCache::EraseIdMapBoundary(Span* pSpan) {
        _IdSpanMap.set(pSpan->_pageId, nullptr);
        if (pSpan->_n > 1) {
            _IdSpanMap.set(pSpan->_pageId + pSpan->_n - 1, nullptr);
        }
    }
    PageCache PageCache::_instance;
};
