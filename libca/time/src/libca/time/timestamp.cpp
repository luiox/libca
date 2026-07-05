#include "libca/time/timestamp.hpp"

namespace ca::time {

Timestamp Timestamp::now() noexcept
{
    return from_time_point(std::chrono::system_clock::now());
}

bool Timestamp::is_past() const noexcept
{
    return *this <= now();
}

bool Timestamp::is_future() const noexcept
{
    return *this > now();
}

Timestamp& Timestamp::operator+=(Duration duration) noexcept
{
    *this = *this + duration;
    return *this;
}

Timestamp& Timestamp::operator-=(Duration duration) noexcept
{
    *this = *this - duration;
    return *this;
}

}  // namespace ca::time
