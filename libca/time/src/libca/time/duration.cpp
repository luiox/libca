#include "libca/time/duration.hpp"

namespace ca::time {

Duration& Duration::operator+=(Duration other) noexcept
{
    *this = *this + other;
    return *this;
}

Duration& Duration::operator-=(Duration other) noexcept
{
    *this = *this - other;
    return *this;
}

}  // namespace ca::time
