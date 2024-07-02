#include <libca/utility/bits.hpp>
namespace libca {
int32_t BitsUitl::lowbit(int32_t x)
{
    return x & -x;
}

}   // namespace libca