#include "libca/time/time_util.hpp"

#include <chrono>

namespace ca::time {

ca::i64 TimeUtil::current_time_millis() noexcept
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now);
    return static_cast<ca::i64>(millis.count());
}

ca::i64 TimeUtil::nano_time() noexcept
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now);
    return static_cast<ca::i64>(nanos.count());
}

}  // namespace ca::time
