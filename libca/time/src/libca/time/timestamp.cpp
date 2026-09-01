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

// Timestamp arithmetic is defined inline to keep pure value operations constexpr.

}   // namespace ca::time
