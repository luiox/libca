#include "libca/time/time_util.hpp"

#include <chrono>

namespace ca::time {

ca::i64 TimeUtil::current_time_millis() noexcept
{
    // system_clock 是壁钟（wall clock），返回 Unix epoch 毫秒，对齐 Java
    // System.currentTimeMillis()，适合记录绝对时间点。
    const auto now    = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now);
    return static_cast<ca::i64>(millis.count());
}

ca::i64 TimeUtil::nano_time() noexcept
{
    // steady_clock 是单调时钟，对齐 Java System.nanoTime()：只能用于计算两次调用
    // 之间的相对耗时，其绝对值无 epoch 含义，不能当作时间戳。
    const auto now   = std::chrono::steady_clock::now().time_since_epoch();
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now);
    return static_cast<ca::i64>(nanos.count());
}

}   // namespace ca::time
