#include "BitsUitl.hpp"

namespace ca {
int32_t BitsUitl::lowbit(int32_t x)
{
    return x & -x;
}

}   // namespace ca