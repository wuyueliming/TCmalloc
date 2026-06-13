#pragma once
#include <mutex>

namespace CMP
{
    // 使用 std::mutex 作为锁原语
    // 后续可替换为更高效的自旋锁实现
    using Lock = std::mutex;
}
