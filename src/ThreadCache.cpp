#include"ThreadCache.h"
#include"CentralCache.h"
#include<algorithm>
namespace CMP
{
    void* ThreadCache::Allocate(size_t size) {
        assert(size <= MAX_BYTES);
        size_t align_num = SizeClass::RoundUp(size);
        size_t index = SizeClass::Index(size);
        //先找现成的内存块
        if (!_FreeLists[index].empty()) {
            return _FreeLists[index].pop();
        }
        else {//没有现成的，从CentralCache批量拿一些过来
        return FetchFromCentralCache(align_num,index);
    }
}
void* ThreadCache::FetchFromCentralCache(size_t size, size_t index) {
    //慢启动，逐渐增加批量移动的对象数量。
    size_t need = std::min(_FreeLists[index].MaxMove(), SizeClass::FetchNumber_obj(size));
    if (need == _FreeLists[index].MaxMove()) {
        _FreeLists[index].MaxMove() += 1;
    }
    void* start, * end;
    start = end = nullptr;
    size_t actual_num = CentralCache::GetInstance()->Allocate(start,end,need,size);
    assert(actual_num > 0);
    assert(start != nullptr && end != nullptr);
    if (actual_num == 1) {
        assert(start == end);
        return start;
    }
    else {
        assert(start != end);
        _FreeLists[index].pushRange(nextptr(start), end, actual_num - 1);
        return start;
    }
}
void ThreadCache::Deallocate(void* ptr, size_t size) {
    if(ptr){
        size_t align_num = SizeClass::RoundUp(size);
        size_t index = SizeClass::Index(size);
        _FreeLists[index].push(ptr);
        //满足一定条件时，回收一些内存到CentralCache
        if(_FreeLists[index].size() >= _FreeLists[index].MaxMove()){
            void* start, * end;
            start = end = nullptr;
            _FreeLists[index].popRange(start, end, _FreeLists[index].MaxMove());
            CentralCache::GetInstance()->Deallocate(start,align_num,_FreeLists[index].MaxMove());
        }
    }
}
}
