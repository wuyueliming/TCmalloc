#pragma once
#include<assert.h>
#include<iostream>
#include<unordered_map>
#include<algorithm>
#include<cstring>
#include <time.h>
#include <thread>
#include <atomic>
#include "base/Lock.h"
static const size_t MAX_BYTES = 256 * 1024;
static const size_t NLISTS = 208;
static const size_t NPAGES = 129;
static const size_t PAGE_SHIFT = 12;
namespace CMP
{
    //#ifdef _WIN64
    //	typedef unsigned long long PAGE_ID;
    //#elif _WIN32
    //	typedef size_t PAGE_ID;
    //#else
    //	// linux
    //#endif
    #if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <windows.h>
    #elif defined(__linux__)
    #define OS_LINUX 1
    #include <unistd.h>
    #include <sys/mman.h>
    #else
    #error "Unsupported operating system"
    #endif
    #if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
    typedef unsigned long long PAGE_ID;
    #else
    typedef size_t PAGE_ID;
    #endif
    // 系统内存分配（按页数）
    inline static void* SystemAlloc(size_t kpage) {
        size_t bytes = kpage << PAGE_SHIFT;
        void* ptr = nullptr;
        #ifdef OS_WINDOWS
        ptr = VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (ptr == nullptr) throw std::bad_alloc();
        #elif  OS_LINUX
        ptr = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) throw std::bad_alloc();
        #endif
        return ptr;
    }
    // 系统内存释放
    inline static void SystemFree(void* ptr, size_t bytes) {
        #ifdef OS_WINDOWS
        VirtualFree(ptr, 0, MEM_RELEASE);
        #elif OS_LINUX
        munmap(ptr, bytes);
        #endif
    }
    //	//系统调用分配内存
    //	inline static void* SystemAlloc(size_t kpage)
    //	{
        //#ifdef _WIN32
        //		void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        //#else
        //		// linux下brk mmap等
        //#endif
        //
        //		if (ptr == nullptr)
        //			throw std::bad_alloc();
        //
        //		return ptr;
        //	}
        //
        //	//系统调用释放内存
        //	inline static void SystemFree(void* ptr)
        //	{
            //#ifdef _WIN32
            //		VirtualFree(ptr, 0, MEM_RELEASE);
            //#else
            //		// sbrk unmmap等
            //#endif
            //	}
            class SizeClass {
                public:
                // 整体控制在最多10%左右的内碎片浪费
                // [1,128]					8byte对齐	    freelist[0,16)
                // [128+1,1024]				16byte对齐	    freelist[16,72)
                // [1024+1,8*1024]			128byte对齐	    freelist[72,128)
                // [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)
                // [64*1024+1,256*1024]		8*1024byte对齐   freelist[184,208)
                //将size按照alignNum向上对齐
                static inline size_t _RoundUp(size_t size, size_t alignNum)
                {
                    return ((size + alignNum - 1) & ~(alignNum - 1));
                }
                static inline size_t RoundUp(size_t size)
                {
                    //小于256KB的请求通过三层内存池
                    if (size <= 128)
                    {
                        return _RoundUp(size, 8);
                    }
                    else if (size <= 1024)
                    {
                        return _RoundUp(size, 16);
                    }
                    else if (size <= 8 * 1024)
                    {
                        return _RoundUp(size, 128);
                    }
                    else if (size <= 64 * 1024)
                    {
                        return _RoundUp(size, 1024);
                    }
                    else if (size <= 256 * 1024)
                    {
                        return _RoundUp(size, 8 * 1024);
                    }
                    else//大于256KB的请求直接通过PageCache分配
                    {
                        return _RoundUp(size, 1 << PAGE_SHIFT);
                    }
                }
                static inline size_t _Index(size_t size, size_t align_shift)
                {
                    return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
                }
                // 计算映射的哪一个自由链表桶
                static inline size_t Index(size_t size)
                {
                    assert(size <= MAX_BYTES);
                    // 每个区间有多少个链
                    static int group_array[4] = { 16, 56, 56, 56 };
                    if (size <= 128) {
                        return _Index(size, 3);
                    }
                    else if (size <= 1024) {
                        return _Index(size - 128, 4) + group_array[0];
                    }
                    else if (size <= 8 * 1024) {
                        return _Index(size - 1024, 7) + group_array[1] + group_array[0];
                    }
                    else if (size <= 64 * 1024) {
                        return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
                    }
                    else if (size <= 256 * 1024) {
                        return _Index(size - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
                    }
                    else {
                        assert(false);
                    }
                    return -1;
                }
                static size_t FetchNumber_obj(size_t size) {
                    assert(size > 0);
                    // [2, 512]，一次批量移动多少个对象的(慢启动)上限值
                    // 小对象一次批量上限高，大对象一次批量上限低
                    int num = MAX_BYTES / size;
                    if (num < 2)
                    num = 2;
                    if (num > 512)
                    num = 512;
                    return num;
                }
                static size_t FetchNumber_page(size_t size) {
                    size_t num = FetchNumber_obj(size);
                    size_t page = (size * num) >> PAGE_SHIFT;
                    if (page == 0) {
                        page = 1;
                    }
                    return page;
                }
            };
            static void*& nextptr(void* ptr) {
                assert(ptr != nullptr);
                return *(void**)ptr;
            }
            class FreeList {
                public:
                FreeList() = default;
                void push(void* ptr) {
                    assert(ptr != nullptr);
                    nextptr(ptr) = _Head;
                    _Head = ptr;
                    _size++;
                }
                void pushRange(void* start, void* end, size_t num) {
                    assert(start != nullptr && end != nullptr);
                    nextptr(end) = _Head;
                    _Head = start;
                    _size += num;
                }
                void popRange(void*& start, void*& end, size_t num) {
                    assert(_Head != nullptr);
                    start = _Head;
                    end = _Head;
                    int i = 0;
                    for (i = 0; i < num - 1 && nextptr(end) != nullptr; i++) {
                        end = nextptr(end);
                    }
                    _Head = nextptr(end);
                    size_t actual_num = i + 1;
                    assert(actual_num == num);
                    _size -= num;
                }
                void * pop() {
                    assert(_Head != nullptr);
                    void* ret = _Head;
                    _Head = nextptr(_Head);
                    _size--;
                    return ret;
                }
                bool empty() const {
                    return _Head == nullptr;
                }
                size_t size() const {
                    return _size;
                }
                size_t& MaxMove() {
                    return _MaxMove;
                }
                private:
                void* _Head = nullptr;
                size_t _size = 0;
                size_t _MaxMove = 1;
            };
            // 管理多个连续页大块内存跨度结构
            struct Span
            {
                PAGE_ID _pageId = 0; // 大块内存起始页的页号
                size_t  _n = 0;      // 页的数量
                Span* _next = nullptr;	// 双向链表的结构
                Span* _prev = nullptr;
                size_t _objSize = 0;  // 切好的小对象的大小
                size_t _useCount = 0; // 切好小块内存，被分配给thread cache的计数
                void* _freeList = nullptr;  // 切好的小块内存的自由链表
                bool _isUse = false;          // 是否在被使用
            };
            class SpanList {
                public:
                SpanList() {
                    _Head = new Span;
                    _Head->_next = _Head;
                    _Head->_prev = _Head;
                }
                Span* Begin() {
                    return _Head->_next;
                }
                Span* End() {
                    return _Head;
                }
                bool empty() const {
                    return _Head->_next == _Head;
                }
                void pushFront(Span* span) {
                    assert(span != nullptr);
                    span->_next = _Head->_next;
                    span->_prev = _Head;
                    _Head->_next->_prev = span;
                    _Head->_next = span;
                }
                void pushBack(Span* span) {
                    assert(span != nullptr);
                    span->_next = _Head;
                    span->_prev = _Head->_prev;
                    _Head->_prev->_next = span;
                    _Head->_prev = span;
                }
                Span* popFront() {
                    assert(!empty());
                    Span* ret = _Head->_next;
                    _Head->_next = ret->_next;
                    ret->_next->_prev = _Head;
                    return ret;
                }
                Span* popBack() {
                    assert(!empty());
                    Span* ret = _Head->_prev;
                    _Head->_prev = ret->_prev;
                    ret->_prev->_next = _Head;
                    return ret;
                }
                void Erase(Span* span) {
                    assert(span != nullptr);
                    if (span->_prev) span->_prev->_next = span->_next;
                    if (span->_next) span->_next->_prev = span->_prev;
                    span->_prev = nullptr;
                    span->_next = nullptr;
                }
                Lock& GetMutex() {
                    return _mutex;
                }
                private:
                Span* _Head;
                Lock _mutex;
            };
        }
