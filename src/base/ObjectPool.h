#pragma once
#include"../common.h"
namespace CMP
{
    // 设计说明：ObjectPool 申请的内存进程级持有，不归还系统（分配器特性）
    template<typename T>
    class ObjectPool
    {
        public:
        ObjectPool() = default;
        ~ObjectPool() = default;
        T* New() {
            _mutex.lock();
            void* ret = nullptr;
            if (_freelist) {
                ret = _freelist;
                _freelist = nextptr(_freelist);
            }
            else {
                if (!_memory || _remainBytes < sizeof(T)) {
                    // 默认申请 16 页 = 64KB，但如果 sizeof(T) 更大则按需分配
                    size_t need_bytes = 16 << PAGE_SHIFT;
                    if (sizeof(T) > need_bytes) {
                        need_bytes = sizeof(T);
                    }
                    size_t kpage = (need_bytes + (1 << PAGE_SHIFT) - 1) >> PAGE_SHIFT;
                    _remainBytes = kpage << PAGE_SHIFT;
                    _memory = SystemAlloc(kpage);
                    align2fit();
                }
                ret = _memory;
                _memory = (char*)_memory + sizeof(T);
                _remainBytes -= sizeof(T);
                align2fit();
            }
            _mutex.unlock();
            new(ret) T;
            return (T*)ret;
        }
        void Delete(T* ptr) {
            _mutex.lock();
            ptr->~T();
            nextptr(ptr) = _freelist;
            _freelist = ptr;
            _mutex.unlock();
        }
        //对齐内存的起始地址
        void align2fit(){
            size_t alignNum = alignof(T);
            char* old_mem = (char*)_memory;
            _memory = (void*)SizeClass::_RoundUp((size_t)_memory,alignNum);
            _remainBytes -= ((char*)_memory - old_mem);  // 扣减对齐消耗
        }
        private:
        void* _memory = nullptr;
        size_t _remainBytes = 0;
        void* _freelist = nullptr;
        Lock _mutex;
    };
}
