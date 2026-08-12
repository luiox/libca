#include "BitsUtil.hpp"

namespace ca {
int32_t BitsUtil::lowbit(int32_t x)
{
    return x & -x;
}

}   // namespace ca
